#!/usr/bin/env sh

if [ ! -d "build/native" ]
then
    meson setup build/native
fi

meson compile -C build/native && ./build/native/main "$@"