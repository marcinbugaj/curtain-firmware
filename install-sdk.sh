#!/bin/bash

set -e

cd /
git clone --branch 2.2.0 --depth 1 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init --depth 1
