#!/bin/bash

# 1. Build the project using CMake Preset
echo "Start Server build using macOS Preset..."
cmake --preset macos-default
cmake --build build_mac

# 2. Run the server
if [ $? -eq 0 ]; then
    echo "Build Success! Run Server..."
    ./build_mac/SecureWebServer
else
    echo "Build Failed, please check error"
fi