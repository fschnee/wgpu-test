#include <fmt/core.h>
#include <fmt/ranges.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <array>

#include <rapidobj/rapidobj.hpp>

#include <imgui.h>
#include "imgui_widgets/group_panel.hpp"

#include "zep.hpp"

#include "webgpu.hpp"
#include "context.hpp"
#include "m.hpp"

#include "standalone/all.hpp"

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

    using vecf = std::vector<float>;
    using vecu = std::vector<u16>;
    using vec3f = std::array<float, 3>;

    struct object
    {
        std::string name = "";

        vecf vertex_data = {};
        vecf color_data = {};
        vecf normal_data = {};
        vecu index_data = {};

        vec3f origin = {0, 0, 0};
        vec3f pos    = {0, 0, 0};
        vec3f rot    = {0, 0, 0}; // In radians.
        vec3f scale  = {1, 1, 1};

        std::function<void(float dt, object& self)> on_update = [](auto, auto){};

        // You can override this if you *really* want to, but you normally shouldn't.
        std::function<m4f(object const&)> compute_transform = [](auto& self){
            return m4f::translation(self.pos[0], self.pos[1], self.pos[2])
                .xRotate(self.rot[0])
                .yRotate(self.rot[1])
                .zRotate(self.rot[2])
                .scale(self.scale[0], self.scale[1], self.scale[2])
                .translate(self.origin[0], self.origin[1], self.origin[2]);
        };
    };

    struct userdata
    {
        u32 nw = 1280;
        u32 nh = 640;

        // Perspective transform stuff.
        float focal_len = 2.0;
        float near = 0.01;
        float far  = 100;
        // View transform (camera) stuff + geometry data from
        // all objects that updates every time geometry changes = true.
        object scene = {
            .pos = {0.0f, 0.0f, 2.0f},
            .rot = {-3.0f * M_PI / 4.0f, 0.0f, 0.0f},
        };
        bool geometry_changed = false;

        float total_seconds = 0.0f;

        std::vector<object> objects;
    } ud = userdata{};

    ud.geometry_changed = true;

    auto p = object{
        .vertex_data = {
            -0.5, -0.5, -0.3,
            +0.5, -0.5, -0.3,
            +0.5, +0.5, -0.3,
            -0.5, +0.5, -0.3,
            -0.5, -0.5, -0.3,
            +0.5, -0.5, -0.3,
            +0.0, +0.0, +0.5,
            +0.5, -0.5, -0.3,
            +0.5, +0.5, -0.3,
            +0.0, +0.0, +0.5,
            +0.5, +0.5, -0.3,
            -0.5, +0.5, -0.3,
            +0.0, +0.0, +0.5,
            -0.5, +0.5, -0.3,
            -0.5, -0.5, -0.3,
            +0.0, +0.0, +0.5,
        },
        .color_data = {
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
            1.0, 1.0, 1.0,
        },
        .normal_data = {
             0.0,   -1.0,   0.0,
             0.0,   -1.0,   0.0,
             0.0,   -1.0,   0.0,
             0.0,   -1.0,   0.0,
             0.0,   -0.848, 0.53,
             0.0,   -0.848, 0.53,
             0.0,   -0.848, 0.53,
             0.848, 0.0,    0.53,
             0.848, 0.0,    0.53,
             0.848, 0.0,    0.53,
             0.0,   0.848,  0.53,
             0.0,   0.848,  0.53,
             0.0,   0.848,  0.53,
            -0.848, 0.0,    0.53,
            -0.848, 0.0,    0.53,
            -0.848, 0.0,    0.53,
        },
        .index_data = {
            // Base
             0,  1,  2,
             0,  2,  3,
            // Sides
             4,  5,  6,
             7,  8,  9,
            10, 11, 12,
            13, 14, 15,
        },
        //.origin = {0.5f, 0.0f, 0.0f},
        //.scale  = {0.3f, 0.3f, 0.3f},
        /* .on_update = [](auto dt, auto self){
                self.rotation += m4f::zRotation(dt);
            }
        }*/
    };

    p.name = "Pyramid";
    p.compute_transform = [](auto& self){
        return m4f::zRotation(self.rot[2])
            .translate(0.5, 0.0, 0.0) // invert origin and scale operation order and hardcode values.
            .scale(0.3, 0.3, 0.3);
    };
    p.on_update = [](auto dt, auto& self) { self.rot[2] += dt; };
    ud.objects.push_back(p);

    ctx.loop(&ud, [](auto dt, auto ctx, auto* _ud)
    {
        auto& ud = *(_ud * cvt::rc<userdata*>);

        ud.total_seconds += dt;

        auto queue = ctx.device.getQueue();

        if(ud.geometry_changed)
        {
            // Rebuild the scene geometry.
            ud.scene.vertex_data.clear();
            ud.scene.color_data.clear();
            ud.scene.normal_data.clear();
            ud.scene.index_data.clear();
            for(const auto& obj : ud.objects)
            {
                ud.scene.vertex_data.insert(ud.scene.vertex_data.end(), obj.vertex_data.begin(), obj.vertex_data.end());
                ud.scene.color_data.insert(ud.scene.color_data.end(),   obj.color_data.begin(),  obj.color_data.end());
                ud.scene.normal_data.insert(ud.scene.normal_data.end(), obj.normal_data.begin(), obj.normal_data.end());
                ud.scene.index_data.insert(ud.scene.index_data.end(),   obj.index_data.begin(),  obj.index_data.end());
            }

            // And upload it to the gpu.
            queue.writeBuffer(ctx.vertex_buffer, 0, ud.scene.vertex_data.data(), ud.scene.vertex_data.size() * sizeof(float));
            queue.writeBuffer(ctx.color_buffer,  0, ud.scene.color_data.data(),  ud.scene.color_data.size()  * sizeof(float));
            queue.writeBuffer(ctx.normal_buffer, 0, ud.scene.normal_data.data(), ud.scene.normal_data.size() * sizeof(float));
            queue.writeBuffer(ctx.index_buffer,  0, ud.scene.index_data.data(),  ud.scene.index_data.size()  * sizeof(u16));
            ud.geometry_changed = false;
        }

        if(ud.nw != ctx.w || ud.nh != ctx.h) { ctx.set_resolution(ud.nw, ud.nh); }

        ud.scene.on_update(dt, ud.scene);
        const auto scene_uniforms = context::scene_uniforms{
            .view       = ud.scene.compute_transform(ud.scene),
            .projection = m4f::perspective({
                .focal_len    = ud.focal_len,
                .aspect_ratio = (ud.nw * cvt::to<float>) / (ud.nh * cvt::to<float>),
                .near         = ud.near,
                .far          = ud.far
            }),

            .light_direction = {0.2f, 0.4f, 0.3f},
            .light_color     = {1.0f, 0.9f, 0.6f},

            .time  = ud.total_seconds,
            .gamma = 2.2,
        };
        ctx.device.getQueue().writeBuffer(ctx.scene_uniform_buffer, 0, &scene_uniforms, sizeof(scene_uniforms));

        // Doing object ticks and updating the uniform buffer.
        for(auto i = 0_u64; i < ud.objects.size(); ++i)
        {
            auto& obj = ud.objects[i];

            obj.on_update(dt, obj);
            const auto object_uniform = context::object_uniforms{ .transform = obj.compute_transform(obj) };
            ctx.device.getQueue().writeBuffer(
                ctx.object_uniform_buffer,
                i * ctx.object_uniform_stride,
                &object_uniform,
                sizeof(object_uniform)
            );
        }

        // UI stuffs.
        ctx.imgui_new_frame();
        { // Scope only for IDE collapsing purposes.
            ImGui::BeginMainMenuBar();
            {
                auto frame_str = fmt::format("Verts: {} Frame {}: {:.1f} FPS -> {:.4f} MS", ud.scene.index_data.size() / 3, ctx.frame, 1 / dt, dt);
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

            if( ImGui::Begin("projection") )
            {
                ImGui::SliderFloat("near", &ud.near, 0.01, 10);
                ImGui::SliderFloat("far", &ud.far, 0, 100);
                ImGui::SliderFloat("focal_len", &ud.focal_len, 0, 10);
                ImGui::SliderFloat3("pos", ud.scene.pos.data(), -1, 1);
                ImGui::SliderFloat3("rot", ud.scene.rot.data(), -3.14, 3.14);
                ImGui::SliderFloat3("sca", ud.scene.scale.data(), 0, 2);

                draw_matrix(scene_uniforms.projection, "Projection", "##projection");
                draw_matrix(scene_uniforms.view, "View", "##view");
                for(auto const& obj : ud.objects)
                    draw_matrix(obj.compute_transform(obj), obj.name.c_str(), obj.name.c_str());
            }
            ImGui::End();
        }

        // Frame rendering stuff.

        auto next_texture = ctx.swapchain.getCurrentTextureView();
        if(!next_texture) return context::loop_message::do_break;

        auto encoder = ctx.device.createCommandEncoder({{ .nextInChain = nullptr, .label = "Command Encoder" }});

        const auto color_attachment = wgpu::RenderPassColorAttachment{{
            .view = next_texture,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{ 0.05, 0.05, 0.05, 1.0 }
        }};
        const auto depth_attachment = wgpu::RenderPassDepthStencilAttachment({
            .view = ctx.depth_texture_view,
            // Operation settings comparable to the color attachment
            .depthLoadOp = wgpu::LoadOp::Clear,
            .depthStoreOp = wgpu::StoreOp::Store,
            // The initial value of the depth buffer, meaning "far"
            .depthClearValue = 1.0f,
            // we could turn off writing to the depth buffer globally here
            .depthReadOnly = false,
            // Stencil setup, mandatory but unused
            .stencilLoadOp = wgpu::LoadOp::Clear,
            .stencilStoreOp = wgpu::StoreOp::Store,
            .stencilClearValue = 0,
            .stencilReadOnly = true,
        });
        auto render_pass = encoder.beginRenderPass({{
            .nextInChain = nullptr,
            .label = "wgpu-test-render-pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = &depth_attachment,
            .occlusionQuerySet = nullptr,
            .timestampWriteCount = 0,
            .timestampWrites = nullptr
        }});

        render_pass.setPipeline(ctx.pipeline);
        render_pass.setBindGroup(0, ctx.scene_bind_group, 0, nullptr);
        render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, ud.scene.vertex_data.size() * sizeof(float));
        render_pass.setVertexBuffer(1, ctx.color_buffer,  0, ud.scene.color_data.size()  * sizeof(float));
        render_pass.setVertexBuffer(2, ctx.normal_buffer, 0, ud.scene.normal_data.size() * sizeof(float));
        render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, ud.scene.index_data.size() * sizeof(u16));

        auto idx_offset = 0_u32;
        auto vertex_offset = 0_i32;
        auto dynamic_bind_offset = 0_u32;
        for(auto const& obj : ud.objects)
        {
            render_pass.setBindGroup(1, ctx.object_bind_group, 1, &dynamic_bind_offset);
            render_pass.drawIndexed(obj.index_data.size(), 1, idx_offset, vertex_offset, 0);
            idx_offset          += obj.index_data.size();
            vertex_offset       += obj.vertex_data.size();
            dynamic_bind_offset += ctx.object_uniform_stride; // TODO: fixme.
        }
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
            ImGui::Text("%.6f", m.raw[i][j]);
    }}
    ImGui::EndTable();
    ImGui::EndGroupPanel();
}