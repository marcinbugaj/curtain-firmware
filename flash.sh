#!/bin/bash

openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program firmware/build/blink.elf verify reset exit"
