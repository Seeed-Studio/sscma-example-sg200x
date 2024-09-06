#!/bin/sh

MD5_FILE=sg2002_recamera_emmc_md5sum.txt
URL_FILE=url.txt
ZIP_FILE=zip.txt

BOOT_PARTITION=/dev/mmcblk0p1
RECV_PARTITION=/dev/mmcblk0p5
ROOTFS1=mmcblk0p3
ROOTFS2=mmcblk0p4
ROOTFS_FILE=rootfs_ext4.emmc

PERCENTAGE=0
PERCENTAGE_FILE=/tmp/upgrade.percentage
CTRL_FILE=/tmp/upgrade.ctrl
VERSION_FILE=/tmp/upgrade.version

function clean_up() {
    if [ ! -z $MOUNTPATH ]; then
        umount $MOUNTPATH
    fi
    rm -rf $CTRL_FILE
    rm -rf $MOUNTPATH
}

function write_percent() {
    echo $PERCENTAGE > $PERCENTAGE_FILE

    if [ -f $CTRL_FILE ]; then
        stop_it=$(cat $CTRL_FILE)
        if [ "$stop_it" = "stop" ]; then
            echo "Stop upgrade."
            clean_up
            exit 2
        fi
    fi
}

function exit_upgrade() {
    write_percent
    clean_up
    exit $1
}

function write_upgrade_flag() {
    echo $1 > $MOUNTPATH/boot
    size=$(blockdev --getsize64 $BOOT_PARTITION)
    offset=$(expr $size / 512)
    offset=$(expr $offset - 1)
    dd if=$MOUNTPATH/boot of=$BOOT_PARTITION bs=512 seek=$offset count=1 conv=notrunc
}

function get_upgrade_url() {
    local url=$1
    local full_url=$url

    if [[ $url =~ .*\.txt$ ]]; then
        full_url=$url
    else
        url=$(curl -skLi $url | grep -i '^location:' | awk '{print $2}' | sed 's/^"//;s/"$//')
        if [ -z "$url" ]; then
            echo ""
            return 1
        fi

        url=$(echo "$url" | sed 's/tag/download/g')
        full_url=$url/$MD5_FILE
    fi

    echo $full_url
    return 0
}

function check_version() {
    issue=""
    if [ -f "/etc/issue" ]; then
        issue=$(cat /etc/issue)
    fi
    if [ -z "$issue" ]; then
        echo "10,No issue file."
        return
    fi

    os_name=$(echo $issue | awk '{print $1}')
    os_version=$(echo $issue | awk '{print $2}')

    if [ $os_name != $1 ]; then
        echo "11,OS name is not match(current:$os_name != $1)."
        return
    else
        if [ $os_version != $2 ]; then
            echo "12,OS version is not match(current:$os_version != $2). "
            return
        else
            echo "0,OS version is match."
        fi
    fi
}

function mount_recovery() {
    fs_type=$(blkid -o value -s TYPE $RECV_PARTITION)
    if [ "$fs_type" != "ext4" ]; then
        mkfs.ext4 $RECV_PARTITION

        # check again
        fs_type=$(blkid -o value -s TYPE $RECV_PARTITION)
        if [ "$fs_type" != "ext4" ]; then
            ehco "Recovery partition is not ext4!"
            return 1
        fi
    fi

    mount $RECV_PARTITION $MOUNTPATH
    if mount | grep -q "$RECV_PARTITION on $MOUNTPATH type"; then
        echo "Mount $RECV_PARTITION on $MOUNTPATH ok."
        return 0
    else
        echo "Mount $RECV_PARTITION on $MOUNTPATH failed."
        return 2
    fi
}

function wget_file() {
    cp /tmp/resolv.conf /tmp/resolv.conf.bak
    sed -i 's/^nameserver 192.168.16.1/#&/' /tmp/resolv.conf
    wget -q --no-check-certificate $1 -O $2
    cp /tmp/resolv.conf.bak /tmp/resolv.conf
}

case $1 in
latest)
    if [ -z "$2" ]; then echo "Usage: $0 start <url>"; exit 1; fi
    if [ -f $CTRL_FILE ]; then echo "Upgrade is running."; exit 1; fi
    echo "" > $CTRL_FILE

    PERCENTAGE=0
    write_percent

    # Get upgrade url
    url_md5=$(get_upgrade_url $2)
    if [ -z $url_md5 ]; then
        PERCENTAGE="1,Unkown url."
        exit_upgrade 1
    fi

    # Download md5_txt.txt
    MOUNTPATH=$(mktemp -d)
    result=$(mount_recovery)
    if [ $? -ne 0 ]; then
        PERCENTAGE=2,"$result"
        exit_upgrade 1
    fi

    # Check version
    md5_txt=$MOUNTPATH/$MD5_FILE
    wget_file $url_md5 $md5_txt

    zip_txt=$MOUNTPATH/$ZIP_FILE
    echo $(cat $md5_txt | grep ".*ota.*\.zip") > $zip_txt
    rm -rf $md5_txt

    zip=$(cat $zip_txt | awk '{print $2}')
    if [ -z "$zip" ]; then
        rm -rfv $zip_txt
        PERCENTAGE="3,Get file list failed."
        exit_upgrade 1
    fi

    os_name=$(echo "$zip" | awk -F'_' '{print $2}')
    os_version=$(echo "$zip" | awk -F'_' '{print $3}')
    if [ -z "$os_name" ] || [ -z "$os_version" ]; then
        rm -rfv $zip_txt
        PERCENTAGE="4,Unknown file name $zip."
        exit_upgrade 1
    fi

    echo "$os_name $os_version" > $VERSION_FILE
    result=$(check_version $os_name $os_version)
    PERCENTAGE=$result

    echo "${url_md5%/*}/$zip" > $MOUNTPATH/$URL_FILE
    exit_upgrade 0
    ;;

start)
    if [ -f $CTRL_FILE ]; then echo "Upgrade is running."; exit 1; fi
    echo "" > $CTRL_FILE

    PERCENTAGE=0
    write_percent

    echo "Step1: Mount recovery partition"
    MOUNTPATH=$(mktemp -d)
    result=$(mount_recovery)
    if [ $? -eq 0 ]; then
        PERCENTAGE=10
    else
        PERCENTAGE=10,"$result"
        exit_upgrade 1
    fi
    write_percent

    echo "Step2: Get upgrade url"
    PERCENTAGE=20
    url_txt=$MOUNTPATH/$URL_FILE
    if [ ! -f $url_txt ]; then
        PERCENTAGE="20,Url.txt not exist."
        exit_upgrade 1
    fi
    full_url=$(cat $url_txt)
    if [ -z "$full_url" ]; then
        PERCENTAGE="20,Url.txt file is empty."
        exit_upgrade 1
    fi
    echo "url: $full_url"

    echo "Step3: Read md5.txt"
    zip_txt=$MOUNTPATH/$ZIP_FILE
    if [ ! -f $zip_txt ]; then
        PERCENTAGE="20,Zip.txt not exist."
        exit_upgrade 1
    fi
    md5=$(cat $zip_txt | awk '{print $1}')
    zip=$(cat $zip_txt | awk '{print $2}')
    echo "zip=$zip"
    echo "md5=$md5"
    if [ -z "$md5" ] || [ -z "$zip" ]; then
        PERCENTAGE="20,Zip.txt file is empty."
        exit_upgrade 1
    fi
    write_percent

    echo "Step4: Download $zip"
    full_path=$MOUNTPATH/$zip
    echo "path: $full_path"
    rm -fv $MOUNTPATH/*.zip
    wget_file $full_url $full_path
    if [ -f $full_path ]; then
        PERCENTAGE=40
    else
        PERCENTAGE=40,"Download failed."
        exit_upgrade 1
    fi
    write_percent

    echo "Step5: Check $zip"
    zip_md5=$(md5sum $full_path | awk '{print $1}')
    echo "zip_md5=$zip_md5"
    if [ "$md5" != "$zip_md5" ]; then
        PERCENTAGE=50,"Package md5 check failed."
        exit_upgrade 1
    fi
    write_percent

    echo "Step6: Read $ROOTFS_FILE md5"
    read_md5=$(unzip -p $full_path md5sum.txt | grep "$ROOTFS_FILE" | awk '{print $1}')
    echo "read_md5=$read_md5"
    PERCENTAGE=60
    # file_md5=$(unzip -p $full_path $ROOTFS_FILE | md5sum | awk '{print $1}')
    # echo "file_md5=$file_md5"
    # if [ "$read_md5" == "$file_md5" ]; then
    #     PERCENTAGE=60
    # else
    #     PERCENTAGE=60,"Package md5 check failed."
    #     exit_upgrade 1
    # fi
    write_percent

    echo "Step7: Writing rootfs"
    rootfs1=$(lsblk 2>/dev/null | grep " /" | grep "$ROOTFS1")
    target=/dev/$ROOTFS2
    if [ -z "$rootfs1" ]; then
        rootfs2=$(lsblk 2>/dev/null | grep " /" | grep "$ROOTFS2")
        if [ -z "$rootfs2" ]; then
            PERCENTAGE=70, "Unknow rootfs partition."
            exit_upgrade 1
        else
            target=/dev/$ROOTFS1
        fi
    fi
    echo "target partition: $target"
    unzip -p $full_path $ROOTFS_FILE | dd of=$target bs=1M
    PERCENTAGE=70
    write_percent

    echo "Step8: Calc $target md5"
    partition_md5=$(dd if=$target bs=1M count=200 2>/dev/null | md5sum | awk '{print $1}')
    echo "partition_md5=$partition_md5"
    PERCENTAGE=80
    write_percent

    echo "Step9: Check $target md5"
    if [ "$partition_md5" == "$read_md5" ]; then
        PERCENTAGE=90
        if [ "$target" == "/dev/$ROOTFS1" ]; then
            write_upgrade_flag "rfs1"
        elif [ "$target" == "/dev/$ROOTFS2" ]; then
            write_upgrade_flag "rfs2"
        fi
    else
        PERCENTAGE=90,"Partition md5 check failed."
        exit_upgrade 1
    fi
    sync
    sleep 1

    PERCENTAGE=100
    echo "Finished!"
    exit_upgrade 0
    ;;

stop)
    echo "stop" > $CTRL_FILE
    ;;

query)
    if [ -f $PERCENTAGE_FILE ]; then
        cat $PERCENTAGE_FILE
    else
        echo "0"
    fi
    ;;

esac
