#!/usr/bin/env sh

## Initial setup.

if ! command -v emcc &> /dev/null
then
    if [ ! -d "3rdparty/emsdk" ]
    then
        mkdir -p 3rdparty/emsdk
        git clone https://github.com/emscripten-core/emsdk 3rdparty/emsdk
    fi

    cd 3rdparty/emsdk
        ./emsdk install latest
        ./emsdk activate latest
        . ./emsdk_env.sh
    cd ../..
fi

# Build proper.

if [ ! -d "build/wasm" ]
then
    meson setup build/wasm --cross-file wasm.ini
fi

meson compile -C build/wasm --verbose
