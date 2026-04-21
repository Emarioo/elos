#!/usr/bin/env bash

# Run script with sudo

if [ ! -z $@ ]; then
    ip link set dev tap0 down
    ip tuntap del dev tap0 mode tap
else
    ip tuntap add dev tap0 mode tap
    ip link set dev tap0 up
fi
