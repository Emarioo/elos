#!/usr/bin/env bash

# Run script with sudo

if [ ! -z $@ ]; then
    ip link set dev tap0 down
    ip tuntap del dev tap0 mode tap
else
    ip tuntap add dev tap0 mode tap
    # Pick an address and subnet that doesn't interfere with something that exists. (in WSL for example don't pick 192.168.0.X which is probably the subnet your Windows Host is connected to, you may not receive or send any packets)
    ip addr add 192.168.100.50/24 dev tap0
    ip link set dev tap0 up
fi
