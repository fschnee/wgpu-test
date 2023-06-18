#include <iostream>

#include <webgpu/webgpu.h>

#define WEBGPU_CPP_IMPLEMENTATION
#ifdef __EMSCRIPTEN__
    #include <wgpu-cpp/emscripten/webgpu.hpp>
#else
    #include <wgpu-cpp/wgpu-native/webgpu.hpp>

    #include <GLFW/glfw3.h>
    #include <glfw3webgpu.h>
#endif

#include "standalone/dont_forget.hpp"

int main()
{
    #ifndef __EMSCRIPTEN__
        if (!glfwInit())
        {
            std::cerr << "Could not initialize GLFW!" << std::endl;
            return 1;
        }
        STANDALONE_DONT_FORGET( glfwTerminate() );

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // We'll use webgpu and not whatever it wants to try to set default.
        auto window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
        if (!window)
        {
            std::cerr << "Could not open window!" << std::endl;
            return 1;
        }
        STANDALONE_DONT_FORGET( glfwDestroyWindow(window) );
    #endif

    #ifdef __EMSCRIPTEN__
        auto instance = wgpu::Instance{nullptr};
    #else
        auto instance = wgpu::createInstance({});
    #endif
    auto adapter = instance.requestAdapter({});
    if(adapter) std::cout << "Adapter created successfully" << std::endl;
    else        std::cerr << "Failed creating adapter" << std::endl;

    #ifndef __EMSCRIPTEN__
        while (!glfwWindowShouldClose(window)) { glfwPollEvents(); }
    #endif

    return 0;
}
