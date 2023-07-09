#include <fmt/core.h>

#include <filesystem>
#include <chrono>

#include <imgui.h>
#include "imgui_widgets/group_panel.hpp"

#include "zep.hpp"

#include "webgpu.hpp"
#include "context.hpp"
#include <glfw3webgpu.h>

#include "standalone/all.hpp"

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE false
    #define PANIC_ON(cond, msg)
    #define ON_NATIVE(code)
#else
    #define IS_NATIVE true
    #define PANIC_ON(cond, msg) if(cond) { fmt::print("{}", msg); return 1; }
    #define ON_NATIVE(code) code
#endif

#define DONT_FORGET(fn) STANDALONE_DONT_FORGET(fn)

int main()
{
    using namespace standalone::integer_aliases;
    namespace cvt = standalone::cvt;

    auto context = ::context{};
    context.init_glfw();
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

    auto adapter_supported_limits = wgpu::SupportedLimits{};
    adapter.getLimits(&adapter_supported_limits);

    auto device = adapter.requestDevice({{
        .nextInChain = nullptr,
        .label = "wgpu-test-device",
        .requiredFeaturesCount = 0,
        .requiredFeatures = nullptr,
        .requiredLimits = nullptr,
        .defaultQueue = { .nextInChain = nullptr, .label = "wgpu-test-default-queue" }
    }});
    DONT_FORGET(device.drop());
    [[maybe_unused]] auto device_error_callback_keepalive = device.setUncapturedErrorCallback([](WGPUErrorType type, char const* message) {
        fmt::print("Uncaptured device error: type {}: {}\n", type, message);
    });

    auto device_supported_limits = wgpu::SupportedLimits{};
    device.getLimits(&device_supported_limits);

    auto queue = device.getQueue();

    auto w = 640_u32;
    auto h = 480_u32;
    auto nw = 1280_u32; // Update these to change resolution, used later.
    auto nh = 640_u32;
    auto swapchainformat = surface.getPreferredFormat(adapter);
    auto swapchain = device.createSwapChain(surface, {{
        .nextInChain = nullptr,
        .label = "wgpu-test-swapchain",
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = swapchainformat,
        .width = w,
        .height = h,
        .presentMode = wgpu::PresentMode::Fifo,
    }});
    DONT_FORGET(swapchain.drop());

    context.init_imgui(device, swapchainformat);
    PANIC_ON(!context.imgui_init_successful, "Could not init ImGui!\n");

    auto const default_shader_code =
        #include "default_shader.hpp.inc"
    ;
    auto shader_code = default_shader_code;

    auto shader_wgsl_desc = WGPUShaderModuleWGSLDescriptor{
        .chain = {.next = nullptr, .sType = wgpu::SType::ShaderModuleWGSLDescriptor},
        .code = shader_code
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

    auto zep_has_init = false;
    DONT_FORGET(if(zep_has_init) zep_destroy());

    auto stopwatch = standalone::chrono::stopwatch{};
    context.loop([&]()
    {
        auto dt = stopwatch.click().last_segment();

        if(nw != w || nh != h)
        {
            context.notify_new_resolution();
            glfwSetWindowSize(context.window, nw, nh);

            w = nw;
            h = nh;
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

        context.imgui_new_frame();
        if(!zep_has_init) {
            zep_init({1.0f, 1.0f});
            zep_load(std::filesystem::current_path() / "src" / "default_shader.hpp.inc");
            zep_has_init = true;
        }
        zep_update();
        zep_show({640, 480});

        ImGui::BeginMainMenuBar();
        {
            auto frame_str = fmt::format("Frame {}: {:.1f} FPS -> {:.4f} MS", context.frame, 1/dt, dt);
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(frame_str.c_str()).x -  ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", frame_str.c_str());
            ImGui::SetCursorPosX(0);

            ImGui::PushItemWidth(100);
            ImGui::InputInt("Width",  &nw * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PushItemWidth(100);
            ImGui::InputInt("Height", &nh * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
        }
        ImGui::EndMainMenuBar();

        if(ImGui::Begin("Adapter Limits"))
        {
            #define STRINGIFY(a) STRINGIFY_IMPL(a)
            #define STRINGIFY_IMPL(a) #a
            #define IMGUI_LIMIT_DISPLAY(name)                                    \
                ImGui::Text(fmt::format(                                         \
                    "{:<41}: {} = {:^20} {:^20}",                                \
                    STRINGIFY(name),                                             \
                    standalone::clean_type_name(decltype(wgpu::Limits::name){}), \
                    adapter_supported_limits.limits.name,                        \
                    device_supported_limits.limits.name                          \
                ).c_str());

            auto const begin = ImGui::GetCursorPos();
            IMGUI_LIMIT_DISPLAY(maxTextureDimension2D);
            IMGUI_LIMIT_DISPLAY(maxTextureDimension3D);
            IMGUI_LIMIT_DISPLAY(maxTextureArrayLayers);
            IMGUI_LIMIT_DISPLAY(maxBindGroups);
            IMGUI_LIMIT_DISPLAY(maxBindingsPerBindGroup);
            IMGUI_LIMIT_DISPLAY(maxDynamicUniformBuffersPerPipelineLayout);
            IMGUI_LIMIT_DISPLAY(maxDynamicStorageBuffersPerPipelineLayout);
            IMGUI_LIMIT_DISPLAY(maxSampledTexturesPerShaderStage);
            IMGUI_LIMIT_DISPLAY(maxSamplersPerShaderStage);
            IMGUI_LIMIT_DISPLAY(maxStorageBuffersPerShaderStage);
            IMGUI_LIMIT_DISPLAY(maxStorageTexturesPerShaderStage);
            IMGUI_LIMIT_DISPLAY(maxUniformBuffersPerShaderStage);
            IMGUI_LIMIT_DISPLAY(maxUniformBufferBindingSize);
            IMGUI_LIMIT_DISPLAY(maxStorageBufferBindingSize);
            IMGUI_LIMIT_DISPLAY(minUniformBufferOffsetAlignment);
            IMGUI_LIMIT_DISPLAY(minStorageBufferOffsetAlignment);
            IMGUI_LIMIT_DISPLAY(maxVertexBuffers);
            IMGUI_LIMIT_DISPLAY(maxBufferSize);
            IMGUI_LIMIT_DISPLAY(maxVertexAttributes);
            IMGUI_LIMIT_DISPLAY(maxVertexBufferArrayStride);
            IMGUI_LIMIT_DISPLAY(maxInterStageShaderComponents);
            IMGUI_LIMIT_DISPLAY(maxInterStageShaderVariables);
            IMGUI_LIMIT_DISPLAY(maxColorAttachments);
            IMGUI_LIMIT_DISPLAY(maxColorAttachmentBytesPerSample);
            IMGUI_LIMIT_DISPLAY(maxComputeWorkgroupStorageSize);
            IMGUI_LIMIT_DISPLAY(maxComputeInvocationsPerWorkgroup);
            IMGUI_LIMIT_DISPLAY(maxComputeWorkgroupSizeX);
            IMGUI_LIMIT_DISPLAY(maxComputeWorkgroupSizeY);
            IMGUI_LIMIT_DISPLAY(maxComputeWorkgroupSizeZ);
            IMGUI_LIMIT_DISPLAY(maxComputeWorkgroupsPerDimension);

            auto const end = ImGui::GetCursorPos();
            ImGui::SetCursorPos({begin.x + 341, begin.y - 10});
            ImGui::BeginGroupPanel("adapter");
            ImGui::InvisibleButton("adapterbtn", {128, 500});
            ImGui::EndGroupPanel();

            ImGui::SetCursorPos({begin.x + 487, begin.y - 10});
            ImGui::BeginGroupPanel("device");
            ImGui::InvisibleButton("devicebtn", {128, 500});
            ImGui::EndGroupPanel();

            ImGui::SetCursorPos(end);
        }
        ImGui::End();

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
        context.imgui_render(render_pass);
        render_pass.end();

        auto command = encoder.finish({{
            .nextInChain = nullptr,
            .label = "wgpu-test-command-buffer"
        }});

        queue.submit(command);

        next_texture.drop();
        swapchain.present();

        return context::loop_message::do_continue;
    });

    return 0;
}
