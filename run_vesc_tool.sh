#!/bin/bash

# Navigate to the VESC Tool directory
cd "$(dirname "$0")"

# Check if we should build for Linux
if [ "$1" == "linux" ] || [ "$1" == "all" ]; then
    # Build for Linux
    echo "Building Linux version..."
    
    # Create Linux build directory if it doesn't exist
    mkdir -p build/lin
    
    # Original Linux build
    echo "Building original Linux version..."
    qmake -config release "CONFIG += release_lin build_original"
    make clean
    make -j8
    rm -rf build/lin/obj
    
    echo "Linux build completed successfully!"
    echo "Linux binary is available in build/lin directory"
fi

# Build for macOS if no argument or "all" is specified
if [ "$1" != "linux" ]; then
    # Delete the existing app if it exists
    if [ -d "build/macos/VESC Tool.app" ]; then
        echo "Removing existing application..."
        rm -rf "build/macos/VESC Tool.app"
    fi

    # Clean and rebuild the app
    echo "Cleaning project..."
    make clean

    echo "Building macOS VESC Tool..."
    qmake -config release "CONFIG += release_macos build_original" && make -j8

    # Open the application
    echo "Opening VESC Tool..."
    open "build/macos/VESC Tool.app"
fi

echo "All builds completed!"
