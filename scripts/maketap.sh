#!/usr/bin/env bash

# Run script with sudo

if [ ! -z $@ ]; then
    sudo ip link set dev tap0 down
    sudo ip tuntap del dev tap0 mode tap
else
    sudo ip tuntap add dev tap0 mode tap
    # Pick an address and subnet that doesn't interfere with something that exists. (in WSL for example don't pick 192.168.0.X which is probably the subnet your Windows Host is connected to, you may not receive or send any packets)
    sudo ip addr add 192.168.100.50/24 dev tap0
    sudo ip link set dev tap0 up
fi
