#!/usr/bin/env bash

set -x

sudo add-apt-repository -y 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-7 main'
sudo apt-get update -qq

# Install libcheck check C testing framework
wget https://github.com/libcheck/check/releases/download/0.12.0/check-0.12.0.tar.gz
tar xvf check-0.12.0.tar.gz
cd check-0.12.0
autoreconf --install
./configure
make
cp src/.libs/*.so* src
cd -

# Install build tools
sudo apt-get install -y build-essential curl unzip git python3 python3-pip python-protobuf
wget -qO- https://developer.arm.com/-/media/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2 | tar xjf - -C /opt

# Install SDL
sudo apt-get install -y libegl1-mesa-dev libgles2-mesa-dev libsdl2-dev libsdl2-image-dev

