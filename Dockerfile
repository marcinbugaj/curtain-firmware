FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    git \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    build-essential \
    python3 \
    clangd \
    pkg-config \
    automake \
    autoconf \
    texinfo \
    libtool \
    libftdi-dev \
    libusb-1.0-0-dev \
    vim \
    perl \
    gdb-multiarch \
  && rm -rf /var/lib/apt/lists/*

COPY install-sdk.sh /
RUN /install-sdk.sh

COPY install-openocd.sh /
RUN /install-openocd.sh

ENV PICO_SDK_PATH=/pico-sdk
