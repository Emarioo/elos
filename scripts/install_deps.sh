#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

if [ ! -f "/etc/nixos" ]; then

    sudo apt install git make python3 gcc
    sudo apt install gcc-mingw-w64-x86-64
    sudo apt install qemu-system-x86
    sudo apt install automake mtools

    pushd $SCRIPT_DIR/../..
    if [ ! -d mkgpt ]; then
        git clone https://github.com/jncronin/mkgpt
    fi
    cd mkgpt

    set +e
    # We get some version errors but
    # we can still compile.
    automake --add-missing
    autoreconf
    ./configure
    make -j

    popd

else

    pushd $SCRIPT_DIR/../..
    if [ ! -d mkgpt ]; then
        git clone https://github.com/jncronin/mkgpt
    fi
    cd mkgpt

    nix-shell -p automake autoconf libtool pkg-config
    set +e
    ./configure
    make -j

    exit
    popd

fi


