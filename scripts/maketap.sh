#!/usr/bin/env bash

# Run script with sudo

if [ ! -z $@ ]; then
    sudo ip link set dev tap0 down
    sudo ip link set dev tap1 down
    sudo ip tuntap del dev tap0 mode tap
    sudo ip tuntap del dev tap1 mode tap
    sudo ip link delete dev br0
else
    sudo ip tuntap add dev tap0 mode tap
    sudo ip tuntap add dev tap1 mode tap

    sudo ip link add name br0 type bridge

    sudo ip link set tap0 master br0
    sudo ip link set tap1 master br0

    # Pick an address and subnet that doesn't interfere with something that exists. (in WSL for example don't pick 192.168.0.X which is probably the subnet your Windows Host is connected to, you may not receive or send any packets)
    # sudo ip addr add 192.168.0.60/24 dev tap0

    sudo ip link set dev tap0 up
    sudo ip link set dev tap1 up
    sudo ip link set dev br0 up
fi
