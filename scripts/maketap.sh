#!/usr/bin/env bash

# Run script with sudo

if [ ! -z $@ ]; then
    ip link set dev tap0 down
    ip tuntap del dev tap0 mode tap
else
    ip tuntap add dev tap0 mode tap
    ip addr add 192.168.0.60/24 dev tap0
    ip link set dev tap0 up
fi
