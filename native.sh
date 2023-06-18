#!/usr/bin/env sh

if [ ! -d "build" ]
then
    meson setup build
fi

meson compile -C build && ./build/main "$@"