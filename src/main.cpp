#include <cstdio>

#include "webgpu.hpp"
#include "context.hpp"

#include "standalone/dont_forget.hpp"

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE false
    #define PANIC_ON(cond, msg)
    #define ON_NATIVE(code)
#else
    #define IS_NATIVE true
    #define PANIC_ON(cond, msg) if(cond) { std::printf(msg); return 1; }
    #define ON_NATIVE(code) code
#endif

#define DONT_FORGET(fn) STANDALONE_DONT_FORGET(fn)

int main()
{
    auto context = init_context();
    PANIC_ON(!context.init_successful, "Could not initialize GLFW!\n");
    PANIC_ON(!context.window,          "Could not open window!\n");

    auto instance = wgpu::Instance{nullptr};
    if constexpr(IS_NATIVE) { instance = wgpu::createInstance({}); }
    DONT_FORGET(instance.drop());

    auto surface = wgpu::Surface{nullptr};
    ON_NATIVE({ surface = glfwGetWGPUSurface(instance, context.window); });

    auto adapter = instance.requestAdapter({{
        .nextInChain = nullptr,
        .compatibleSurface = surface,
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .forceFallbackAdapter = false
    }});
    PANIC_ON(!adapter, "Failed creating adapter\n");
    DONT_FORGET(adapter.drop());

    auto device = adapter.requestDevice({{
        .nextInChain = nullptr,
        .label = "wgpu-test-device",
        .requiredFeaturesCount = 0,
        .requiredFeatures = nullptr,
        .requiredLimits = nullptr,
        .defaultQueue = { .nextInChain = nullptr, .label = "wgpu-test-default-queue" }
    }});
    DONT_FORGET(device.drop());

    auto queue = device.getQueue();

    auto w = 640;
    auto h = 480;
    auto swapchain = device.createSwapChain(surface, {{
        .nextInChain = nullptr,
        .label = "wgpu-test-swapchain",
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = surface.getPreferredFormat(adapter),
        .width = w,
        .height = h,
        .presentMode = wgpu::PresentMode::Fifo,
    }});
    DONT_FORGET(swapchain.drop());

    context.loop([&](auto new_w, auto new_h)
    {
        if(new_w != w || new_h != h)
        {
            w = new_w;
            h = new_h;
            swapchain.drop();
            swapchain = device.createSwapChain(surface, {{
                .nextInChain = nullptr,
                .label = "wgpu-test-swapchain",
                .usage = wgpu::TextureUsage::RenderAttachment,
                .format = surface.getPreferredFormat(adapter),
                .width = w,
                .height = h,
                .presentMode = wgpu::PresentMode::Fifo,
            }});
        }

        auto next_texture = swapchain.getCurrentTextureView();
        if(!next_texture) return context::loop_message::do_break;

        auto encoder = device.createCommandEncoder({{ .nextInChain = nullptr, .label = "Command Encoder" }});

        auto color_attachment = wgpu::RenderPassColorAttachment{{
            .view = next_texture,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 }
        }};
        auto render_pass = encoder.beginRenderPass({{
            .nextInChain = nullptr,
            .label = "wgpu-test-render-pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = nullptr,
            .occlusionQuerySet = nullptr,
            .timestampWriteCount = 0,
            .timestampWrites = nullptr
        }});
        render_pass.end();

        next_texture.drop();

        auto command = encoder.finish({{
            .nextInChain = nullptr,
            .label = "wgpu-test-command-buffer"
        }});

        queue.submit(command);

        swapchain.present();

        return context::loop_message::do_continue;
    });

    return 0;
}
