#pragma once

// Provided by emscripten on emscripten builds and wgpu-native on native builds.
#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
    #include <wgpu-cpp/emscripten/webgpu.hpp>
#else
    #include <wgpu-cpp/wgpu-native/webgpu.hpp>
#endif
