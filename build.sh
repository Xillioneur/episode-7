#!/bin/bash

# Exit on error
set -e

echo "--- Building Neon Nexus (C++23) ---"

# Clean build directory to avoid cache conflicts
rm -rf build
mkdir -p build
cd build

# Set compiler to MacPorts GCC 15
COMPILER=/opt/local/bin/g++-mp-15

if [ ! -f "$COMPILER" ]; then
    echo "Error: $COMPILER not found. Please ensure gcc15 is installed via MacPorts."
    exit 1
fi

echo "Configuring with CMake using $COMPILER..."
cmake .. -DCMAKE_CXX_COMPILER="$COMPILER"

# Build
echo "Compiling..."
cmake --build .

echo "--- Build Complete ---"
echo "Executable located at: build/neon_nexus"
