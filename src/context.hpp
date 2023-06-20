#pragma once

#ifndef __EMSCRIPTEN__
    #include <GLFW/glfw3.h>
    #include <glfw3webgpu.h>

    struct context
    {
        bool init_successful = false;
        GLFWwindow* window   = nullptr;

        ~context()
        {
            if(window)          glfwDestroyWindow(window);
            if(init_successful) glfwTerminate();
        }

        auto loop(auto fn) { while (!glfwWindowShouldClose(window)) { glfwPollEvents(); fn(); } }
    };

#else

    struct context {
        auto loop(auto fn) { while(true); }
    }; // TODO: create context on emscripten and implement loop.

#endif

auto init_context() -> context
{
    #ifndef __EMSCRIPTEN__
        if (!glfwInit()) return {};

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // We'll use webgpu and not whatever it wants to try to set default.
        auto window = glfwCreateWindow(640, 480, "wgpu-test", nullptr, nullptr);

        return { .init_successful = true, .window = window };
    #else
        return {};
    #endif
}
