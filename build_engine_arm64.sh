#!/usr/bin/env bash
set -e

echo "=========================================================="
echo " Outlast ARM64 Batch Engine Compilation Pipeline"
echo " Target: arm64-v8a (Android NDK r27, API 34)"
echo "=========================================================="

BUILD_DIR="/home/ang/outlast-recomp/build_android"
cd "$BUILD_DIR"

# Compile outlast_lifted (the 400 lifted C++ translation units) using ninja with 3 parallel workers
# to respect server memory while maximizing CPU throughput.
echo "[*] Compiling 400 Lifted Engine Translation Units (gen_*.cpp)..."
ninja -C "$BUILD_DIR" outlast_lifted -j3

echo "[*] outlast_lifted compiled successfully!"
ls -lh "$BUILD_DIR/liboutlast_lifted.a"
