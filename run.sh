#!/usr/bin/env sh

## Initial setup.

if [ ! -d "include" ]
then
    mkdir include
    ln -sf $(pwd)/3rdparty/webgpu-cpp/dawn $(pwd)/include/webgpu
fi

if [ ! -d "3rdparty" ]
then
    mkdir 3rdparty
    git clone https://github.com/emscripten-core/emsdk 3rdparty/emsdk
    # Using C++ version instead of bare C.
    # git clone https://github.com/webgpu-native/webgpu-headers 3rdparty/webgpu-headers
    git clone https://github.com/eliemichel/WebGPU-Cpp 3rdparty/webgpu-cpp
    
    git clone https://dawn.googlesource.com/dawn       3rdparty/dawn
    cd 3rdparty/dawn
        cp scripts/standalone.gclient .gclient
        .gclient sync
        mkdir -p out/Debug
        cd out/Debug
            cmake ../..
            make 
        cd ../..
    cd ../..
fi

if ! command -v emcc &> /dev/null
then
    cd 3rdparty/emsdk
        ./emsdk install latest
        ./emsdk activate latest
        . ./emsdk_env.sh
    cd ../..
fi

if [ ! -d "build" ]
then
    mkdir build
fi

## Build proper.

SOURCES="src/main.cpp src/webgpu.cpp"
CFLAGS="-I$(pwd)/include -Wall -Wextra -Wpedantic"

emcc $SOURCES $CFLAGS -o build/main.html