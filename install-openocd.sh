#!/bin/bash

set -e

cd /
git clone https://github.com/raspberrypi/openocd.git --recursive --branch sdk-2.0.0 --depth=1
cd openocd
./bootstrap
./configure --enable-ftdi --enable-sysfsgpio --enable-bcm2835gpio
make -j$(nproc)
make install
