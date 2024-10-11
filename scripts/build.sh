#!/bin/bash

SG200X_PATH=$PWD
SG200X_BUILD_PATH=$SG200X_PATH/build/
SG200X_SUPERVISOR_BUILD_PATH=$SG200X_PATH/build/supervisor/
SG200X_SSCMANODE_BUILD_PATH=$SG200X_PATH/build/sscma-node/
SG200X_INSTALL_PATH=$SG200X_PATH/install/
SG200X_SUPERVISOR_PATH=$SG200X_PATH/solutions/supervisor/
SG200X_SSCMANODE_PATH=$SG200X_PATH/solutions/sscma-node/

function buildSupervisor() {
    cd $SG200X_PATH

    if [ -d $SG200X_SUPERVISOR_BUILD_PATH ]; then
        rm -r $SG200X_SUPERVISOR_BUILD_PATH
    fi

    mkdir -p $SG200X_SUPERVISOR_BUILD_PATH && cd $SG200X_SUPERVISOR_BUILD_PATH

    cmake $SG200X_SUPERVISOR_PATH && cmake --build . -j3
}

function buildSscmaNode() {
    cd $SG200X_PATH

    if [ -d $SG200X_SSCMANODE_BUILD_PATH ]; then
        rm -r $SG200X_SSCMANODE_BUILD_PATH
    fi

    mkdir -p $SG200X_SSCMANODE_BUILD_PATH && cd $SG200X_SSCMANODE_BUILD_PATH

    cmake $SG200X_SSCMANODE_PATH && cmake --build . -j3
}

function installSupervisor() {
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
    cp -r $SG200X_SUPERVISOR_BUILD_PATH/supervisor mnt/system/usr/bin/
}

function installSscmaNode() {
    cd $SG200X_INSTALL_PATH

    mkdir -p usr/local/bin/

    cp $SG200X_SSCMANODE_PATH/files/etc/init.d/S91sscma-node etc/init.d/
    cp $SG200X_SSCMANODE_BUILD_PATH/sscma-node usr/local/bin/
}

function build() {
    buildSupervisor
    buildSscmaNode
}

function install() {
    installSupervisor
    installSscmaNode

    cd $SG200X_INSTALL_PATH
    tar zcf $SG200X_BUILD_PATH/reCameraApp.tar.gz ./*
    echo "finished"
}

build
install
