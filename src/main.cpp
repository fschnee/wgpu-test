#include <cstdio>

#include "webgpu.hpp"
#include "context.hpp"

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE false
    #define PANIC_ON(cond, msg)
#else
    #define IS_NATIVE true
    #define PANIC_ON(cond, msg) if(cond) { std::printf(msg); return 1; }
#endif

int main()
{
    auto context = init_context();
    PANIC_ON(!context.init_successful, "Could not initialize GLFW!\n");
    PANIC_ON(!context.window,          "Could not open window!\n");

    auto instance = wgpu::Instance{nullptr};
    if constexpr(IS_NATIVE) { instance = wgpu::createInstance({}); }

    auto adapter = instance.requestAdapter({});
    PANIC_ON(!adapter, "Failed creating adapter\n");

    context.loop([&]{

    });

    return 0;
}
