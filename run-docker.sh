#!/bin/bash

docker run --rm -it --network host --mount type=bind,source="$(pwd)"/firmware,target=/firmware -w /firmware curtain-firmware:latest
