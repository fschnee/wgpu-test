#include <cstdio>

#include "webgpu.hpp"
#include "context.hpp"

#include "standalone/dont_forget.hpp"
#include "standalone/aliases.hpp"

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

    using namespace s::integer_aliases;
    auto w = 640_u32;
    auto h = 480_u32;
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

    auto shader_wgsl_desc = WGPUShaderModuleWGSLDescriptor{
        .chain = {.next = nullptr, .sType = wgpu::SType::ShaderModuleWGSLDescriptor},
        .code = R"(
            @vertex
            fn vert(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
                var p = vec2f(0.0, 0.0);
                if      (in_vertex_index == 0u) { p = vec2f(-0.5, -0.5); }
                else if (in_vertex_index == 1u) { p = vec2f(0.5, -0.5); }
                else                            { p = vec2f(0.0, 0.5); }
                return vec4f(p, 0.0, 1.0);
            }

            @fragment
            fn frag() -> @location(0) vec4f {
                return vec4f(0.0, 0.4, 1.0, 1.0);
            }
        )",
    };

    auto shader_desc = WGPUShaderModuleDescriptor{
        .nextInChain = &shader_wgsl_desc.chain, // Need to fallback to wgpu-native since wgpu-cpp sets nextInChain to nullptr.
        .label = "wgpu-test-shader",
        .hintCount = 0,
        .hints = nullptr
    };

    auto shader = wgpu::ShaderModule{ wgpuDeviceCreateShaderModule(device, &shader_desc) };

    auto blend_state = wgpu::BlendState{{
        .color = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::SrcAlpha,
            .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha
        },
        .alpha = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::Zero,
            .dstFactor = wgpu::BlendFactor::One
        }
    }};

    auto color_target = wgpu::ColorTargetState{{
        .nextInChain = nullptr,
        .format = surface.getPreferredFormat(adapter),
        .blend = &blend_state,
        .writeMask = wgpu::ColorWriteMask::All
    }};

    auto fragment_state = wgpu::FragmentState{{
        .nextInChain = nullptr,
        .module = shader,
        .entryPoint = "frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &color_target
    }};

    auto pipeline = device.createRenderPipeline({{
        .nextInChain = nullptr,
        .label = "wgpu-test-pipeline",
        .layout = nullptr,
        .vertex = {
            .nextInChain = nullptr,
            .module = shader,
            .entryPoint = "vert",
            .constantCount = 0,
            .constants = nullptr,
            .bufferCount = 0,
            .buffers = nullptr
        },
        .primitive = {
            .nextInChain = nullptr,
            .topology = wgpu::PrimitiveTopology::TriangleList,
            .stripIndexFormat = wgpu::IndexFormat::Undefined,
            .frontFace = wgpu::FrontFace::CCW,
            .cullMode = wgpu::CullMode::None
        },
        .depthStencil = nullptr,
        .multisample = {
            .nextInChain = nullptr,
            .count = 1,
            .mask = ~0u,
            .alphaToCoverageEnabled = false
        },
        .fragment = &fragment_state
    }});

    context.loop([&](u32 new_w, u32 new_h)
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
        render_pass.setPipeline(pipeline);
        render_pass.draw(3, 1, 0, 0);
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
