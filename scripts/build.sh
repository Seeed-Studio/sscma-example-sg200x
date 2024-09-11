#!/bin/bash

SG200X_PATH=$PWD
SG200X_BUILD_PATH=$SG200X_PATH/build/
SG200X_INSTALL_PATH=$SG200X_PATH/install/
SG200X_SUPERVISOR_PATH=$SG200X_PATH/solutions/supervisor/

function build() {
    cd $SG200X_PATH

    if [ -d $SG200X_BUILD_PATH ]; then
        rm -r $SG200X_BUILD_PATH
    fi

    mkdir $SG200X_BUILD_PATH && cd $SG200X_BUILD_PATH

    cmake $SG200X_SUPERVISOR_PATH && cmake --build . -j3
}

function install() {
    cd $SG200X_PATH

    if [ -d $SG200X_INSTALL_PATH ]; then
        rm -r $SG200X_INSTALL_PATH
    fi

    mkdir $SG200X_INSTALL_PATH && cd $SG200X_INSTALL_PATH

    mkdir -p etc/init.d/
    mkdir -p mnt/system/usr/bin/

    cp $SG200X_PATH/scripts/S93sscma-supervisor etc/init.d/
    cp -r $SG200X_SUPERVISOR_PATH/main/scripts/ mnt/system/usr/
    cp -r $SG200X_SUPERVISOR_PATH/main/dist/ mnt/system/usr/
    cp -r $SG200X_BUILD_PATH/supervisor mnt/system/usr/bin/
    # ln -s ./usr/bin/supervisor mnt/system/default_app

    tar zcf $SG200X_BUILD_PATH/supervisor.tar.gz ./*
    echo "finished"
}

build
install
