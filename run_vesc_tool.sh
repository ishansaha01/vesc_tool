#!/bin/bash

# Navigate to the VESC Tool directory
cd "$(dirname "$0")"

# Delete the existing app if it exists
if [ -d "build/macos/VESC Tool.app" ]; then
    echo "Removing existing application..."
    rm -rf "build/macos/VESC Tool.app"
fi

# Clean and rebuild the app
echo "Cleaning project..."
make clean

echo "Building VESC Tool..."
qmake -config release "CONFIG += release_macos build_original" && make -j8

# Open the application
echo "Opening VESC Tool..."
open "build/macos/VESC Tool.app"
