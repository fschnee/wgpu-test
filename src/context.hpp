#pragma once

#include "webgpu.hpp"

#include <GLFW/glfw3.h>

#include "standalone/aliases.hpp"

struct context
{
    bool init_successful       = false;
    GLFWwindow* window         = nullptr;
    bool imgui_init_successful = false;
    bool new_resolution        = false;

    standalone::u64 frame = 0;

    ~context();

    auto imgui_new_frame() -> void;
    auto imgui_render(WGPURenderPassEncoder) -> void;

    auto init_glfw() -> context&;
    auto init_imgui(WGPUDevice, WGPUTextureFormat swapchainformat, WGPUTextureFormat depthbufferformat = WGPUTextureFormat_Undefined) -> context&;

    auto notify_new_resolution() -> context&;

    enum class loop_message {do_break, do_continue};

    auto loop(auto fn)
    {
        #ifndef __EMSCRIPTEN__
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                if(fn() == loop_message::do_break) break;
            }
        #else
            while(true) fn(); // TODO: implement loop.
        #endif
    }
};
