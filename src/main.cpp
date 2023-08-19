#include <fmt/core.h>

#include <filesystem>

#include <rapidobj/rapidobj.hpp>

#include <imgui.h>
#include "imgui_widgets/group_panel.hpp"

#include "zep.hpp"

#include "webgpu.hpp"
#include "context.hpp"

#include "standalone/all.hpp"

#include "m.hpp"

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE false
    #define PANIC_ON(cond, msg)
#else
    #define IS_NATIVE true
    #define PANIC_ON(cond, msg) if(cond) { fmt::print("{}", msg); return 1; }
#endif

#define DONT_FORGET(fn) STANDALONE_DONT_FORGET(fn)

auto draw_limits_window(wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void;
auto draw_matrix(m4f const&, const char*, const char*) -> void;

int main()
{
    using namespace standalone::integer_aliases;
    namespace cvt = standalone::cvt;

    bool ok = true;
    auto ctx = context::get().init_all([&]([[maybe_unused]] auto& err, [[maybe_unused]] auto& ctx){
        ok = false;
        // TODO: catch init errors
    });
    if(!ok) return 1;

    struct userdata {
        bool zep_has_init = false;
        u32 nw = 1280;
        u32 nh = 640;

        float depth = 200;
        float translation[3] = {0, 0, 0};
        float rotation[3] = {0, 0, 0};
        float scale[3] = {1, 1, 1};
        bool use_transform = true;

        float total_seconds = 0.0f;

        std::vector<float> vertex_data = {
            -0.5, -0.5, -0.3,
            +0.5, -0.5, -0.3,
            +0.5, +0.5, -0.3,
            -0.5, +0.5, -0.3,
            +0.0, +0.0, +0.5
        };
        std::vector<float> color_data = {
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            0.5, 0.5, 0.5
        };
        std::vector<u16> index_data = {
            0, 1, 2,
            0, 2, 3,
            0, 1, 4,
            1, 2, 4,
            2, 3, 4,
            3, 0, 4
        };
        bool geometry_changed = true;
    } ud = userdata{};

    ctx.loop(&ud, [](auto dt, auto ctx, auto* _ud)
    {
        auto& ud = *(_ud * cvt::rc<userdata*>);

        ud.total_seconds += dt;

        if(ud.geometry_changed)
        {
            auto queue = ctx.device.getQueue();
            queue.writeBuffer(ctx.vertex_buffer, 0, ud.vertex_data.data(), ud.vertex_data.size() * sizeof(float));
            queue.writeBuffer(ctx.color_buffer,  0, ud.color_data.data(),  ud.color_data.size()  * sizeof(float));
            queue.writeBuffer(ctx.index_buffer,  0, ud.index_data.data(),  ud.index_data.size()  * sizeof(u16));
            ud.geometry_changed = false;
        }

        if(ud.nw != ctx.w || ud.nh != ctx.h) { ctx.set_resolution(ud.nw, ud.nh); }

        // Ignoring projection for now.
        auto transform = m4f::translation(0, 0, 0) //m4f::projection(ud.nw, ud.nh, ud.depth)
            .translate(ud.translation[0], ud.translation[1], ud.translation[2])
            .xRotate(ud.rotation[0])
            .yRotate(ud.rotation[1])
            .zRotate(ud.rotation[2])
            .scale(ud.scale[0], ud.scale[1], ud.scale[2]);

        const auto uniform = context::uniforms{
            .transform = ud.use_transform ? transform : m4f::scaling(1, 1, 1),
            .time = ud.total_seconds
        };

        ctx.device.getQueue().writeBuffer(ctx.uniform_buffer, 0, &uniform, sizeof(uniform));
        ctx.imgui_new_frame();

        /*
        if(!ud.zep_has_init) {
            zep_init({1.0f, 1.0f});
            zep_load(std::filesystem::current_path() / "src" / "default_shader.hpp.inc");
            ud.zep_has_init = true;
        }
        zep_update();
        zep_show({300, 300});
        */

        ImGui::BeginMainMenuBar();
        {
            auto frame_str = fmt::format("Verts/Indexes: {}/{} Frame {}: {:.1f} FPS -> {:.4f} MS", ud.vertex_data.size() / 3, ud.index_data.size() / 3, ctx.frame, 1/dt, dt);
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(frame_str.c_str()).x -  ImGui::GetStyle().ItemSpacing.x);
            ImGui::Text("%s", frame_str.c_str());
            ImGui::SetCursorPosX(0);

            ImGui::PushItemWidth(100);
            ImGui::InputInt("Width",  &ud.nw * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PushItemWidth(100);
            ImGui::InputInt("Height", &ud.nh * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
        }
        ImGui::EndMainMenuBar();

        draw_limits_window(ctx.limits.adapter, ctx.limits.device);

        if( ImGui::Begin("transform") )
        {
            if(ImGui::Button(ud.use_transform ? "Using transform" : "Not using transform"))
                ud.use_transform = !ud.use_transform;
            ImGui::SliderFloat("depth", &ud.depth, 0, 400);
            ImGui::SliderFloat3("pos", ud.translation, -1, 1);
            ImGui::SliderFloat3("rot", ud.rotation, -3.14, 3.14);
            ImGui::SliderFloat3("sca", ud.scale, 0, 2);

            draw_matrix(transform, "Transform", "##transform");
        }
        ImGui::End();

        auto next_texture = ctx.swapchain.getCurrentTextureView();
        if(!next_texture) return context::loop_message::do_break;

        auto encoder = ctx.device.createCommandEncoder({{ .nextInChain = nullptr, .label = "Command Encoder" }});

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

        render_pass.setPipeline(ctx.pipeline);
        render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, ud.vertex_data.size() * sizeof(float));
        render_pass.setVertexBuffer(1, ctx.color_buffer, 0, ud.color_data.size() * sizeof(float));
        render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, ud.index_data.size() * sizeof(u16));
        render_pass.setBindGroup(0, ctx.bind_group, 0, nullptr);
        render_pass.drawIndexed(ud.index_data.size(), 1, 0, 0, 0);
        ctx.imgui_render(render_pass);
        render_pass.end();

        auto command = encoder.finish({{
            .nextInChain = nullptr,
            .label = "wgpu-test-command-buffer"
        }});

        ctx.device.getQueue().submit(command);

        next_texture.drop();
        ctx.swapchain.present();

        return context::loop_message::do_continue;
    });

    return 0;
}

auto draw_limits_window(wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void
{
    if(ImGui::Begin("Adapter Limits"))
    {
        #define STRINGIFY(a) STRINGIFY_IMPL(a)
        #define STRINGIFY_IMPL(a) #a
        #define IMGUI_LIMIT_DISPLAY(name)                                    \
            ImGui::Text(fmt::format(                                         \
                "{:<41}: {} = {:^20} {:^20}",                                \
                STRINGIFY(name),                                             \
                standalone::clean_type_name(decltype(wgpu::Limits::name){}), \
                adapter.limits.name,                                         \
                device.limits.name                                           \
            ).c_str());

        auto const begin = ImGui::GetCursorPos();
        IMGUI_LIMIT_DISPLAY(maxTextureDimension1D);
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
        ImGui::InvisibleButton("adapterbtn", {128, 520});
        ImGui::EndGroupPanel();

        ImGui::SetCursorPos({begin.x + 487, begin.y - 10});
        ImGui::BeginGroupPanel("device");
        ImGui::InvisibleButton("devicebtn", {128, 520});
        ImGui::EndGroupPanel();

        ImGui::SetCursorPos(end);
    }
    ImGui::End();
}

auto draw_matrix(m4f const& m, const char* panelname, const char* tablename) -> void
{
    ImGui::BeginGroupPanel(panelname);
    ImGui::BeginTable(tablename, 4, 0, ImVec2(250.0f, 0.0f));
    for(auto i = 0; i < 4; ++i) {
        for(auto j = 0; j < 4; ++j) {
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", m.raw[i][j]);
    }}
    ImGui::EndTable();
    ImGui::EndGroupPanel();
}