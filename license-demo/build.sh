#!/bin/bash
# Build script for Linux/macOS (uses g++/clang)
set -e
mkdir -p build
cd build
CXX=${CXX:-g++}
echo "Using: $CXX"
$CXX -std=c++17 -O2 -I.. \
    ../license_checker.cpp \
    ../main.cpp \
    -o license_demo \
    -lssl -lcrypto -lcurl
echo "Built: build/license_demo"
