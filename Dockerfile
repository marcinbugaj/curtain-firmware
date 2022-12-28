FROM ubuntu:22.04

RUN apt update && apt install -y \
  git \
  cmake \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libstdc++-arm-none-eabi-newlib \
  build-essential \
  automake \
  autoconf \
  build-essential \
  texinfo \
  libtool \
  libftdi-dev \
  libusb-1.0-0-dev \
  clangd

COPY install-sdk.sh /
RUN /install-sdk.sh

RUN apt update && apt install -y \
  pkg-config

COPY install-openocd.sh /
RUN /install-openocd.sh

RUN apt update && apt install -y \
  python3 \
  vim \
  perl

RUN apt update && apt install -y \
  gdb-multiarch

ENV PICO_SDK_PATH=/pico-sdk
