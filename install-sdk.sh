#!/bin/bash

set -e

cd /
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git checkout 2e6142b15b8a75c1227dd3edbe839193b2bf9041
git submodule update --init
