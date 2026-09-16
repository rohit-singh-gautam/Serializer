#!/bin/bash

CURRENTDIR=$(pwd)

sudo apt update
sudo apt upgrade
# Generated C++ tests and examples require clang-format 19 or newer.
sudo apt install g++ cmake make gdb git ca-certificates pkg-config autoconf ninja-build \
  clang-format-19 curl zip unzip tar

cd ~/
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

echo "export VCPKG_ROOT=$HOME/vcpkg" >> ~/.bashrc
echo "export PATH=\$VCPKG_ROOT:\$PATH" >> ~/.bashrc

./bootstrap-vcpkg.sh

cd $CURRENTDIR

echo Install CMake extension in Visual Studio Code
echo To overcome bug is Visual Code do following
echo Goto CMake Tab, next to "PROJECT ONLINE" click on build all
