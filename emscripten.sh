#!/usr/bin/env sh

## Initial setup.

if ! command -v emcc &> /dev/null
then
    if [ ! -d "3rdparty/emsdk" ]
        mkdir -p 3rdparty/emsdk
        git clone https://github.com/emscripten-core/emsdk 3rdparty/emsdk
    then

    cd 3rdparty/emsdk
        ./emsdk install latest
        ./emsdk activate latest
        . ./emsdk_env.sh
    cd ../..
fi

# TODO: Build proper.

if [ ! -d "build" ]
then
    meson setup build
fi

meson compile -C build