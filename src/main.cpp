#include <iostream>

#include <webgpu/webgpu.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <wgpu-cpp/wgpu-native/webgpu.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include "standalone/dont_forget.hpp"

int main()
{
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

    auto instance = wgpu::createInstance({});
    auto adapter = instance.requestAdapter({});
    std::cout << "Adapter created successfully" << std::endl;

    while (!glfwWindowShouldClose(window)) { glfwPollEvents(); }

    return 0;
}
