#!/usr/bin/env bash

SRC=$(dirname $BASH_SOURCE)

mkdir -p int
gcc -g $SRC/udp.c -o int/udp && int/udp
