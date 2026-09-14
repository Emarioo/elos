#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -e

if [ ! -f "/etc/nixos" ]; then

    sudo apt install p7zip

    pushd $SCRIPT_DIR/../..
    if [ ! -d doomgeneric ]; then
        git clone https://github.com/Emarioo/doomgeneric
    fi
    if [ ! -d iwad ]; then
        git clone https://github.com/Gaytes/iwad
    fi

    # Extract WAD file (DOOM game data)
    cd iwad
    7z e doom1.7z
    
    popd

else

    nix-shell -p p7zip

    pushd $SCRIPT_DIR/../..
    if [ ! -d doomgeneric ]; then
        git clone https://github.com/Emarioo/doomgeneric
    fi
    if [ ! -d iwad ]; then
        git clone https://github.com/Gaytes/iwad
    fi

    # Extract WAD file (DOOM game data)
    cd iwad
    7z e doom1.7z

    exit
    
    popd

fi
