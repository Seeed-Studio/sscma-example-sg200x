#!/bin/sh

change_name() {
    TEMP_FILE=$(mktemp)
    chmod 660 $TEMP_FILE
    sed "s/$1:x/$2:x/g" /etc/passwd > "$TEMP_FILE" && cp "$TEMP_FILE" /etc/passwd
    sed "s/$1:/$2:/g" /etc/shadow > "$TEMP_FILE" && cp "$TEMP_FILE" /etc/shadow
    rm $TEMP_FILE
}

change_passwd() {
    TEMP_FILE=$(mktemp)
    TEMP_ERR_FILE=$(mktemp)
    passwd > $TEMP_FILE 2> $TEMP_ERR_FILE << EOF
$1
$2
$2
EOF

    res=`cat $TEMP_ERR_FILE | grep "unchanged"`

    if [ -z "$res" ]; then
        echo "OK"
    else
        echo "ERROR"
        cat $TEMP_FILE | tail -1
    fi

    rm $TEMP_FILE
    rm $TEMP_ERR_FILE
}

cal_key() {
    TEMP_FILE=$(mktemp)
    echo "$1" > $TEMP_FILE
    ssh-keygen -lf $TEMP_FILE | awk '{print $2}'
    rm $TEMP_FILE
}

query_key() {
    filename=/root/.ssh/authorized_keys
    while IFS= read -r line; do
        cal_key "$line"
    done < "$filename"
}

case $1 in
name)
    change_name $2 $3
    ;;

passwd)
    change_passwd $2 $3
    ;;

query_key)
    query_key
    ;;
esac
