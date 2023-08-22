#include <fmt/core.h>
#include <fmt/ranges.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <array>

#include <omp.h>

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

auto draw_limits_window(bool& open, wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void;
auto draw_matrix(m4f const&, const char*, const char*) -> void;

int main()
{
    using namespace standalone::integer_aliases;
    namespace cvt = standalone::cvt;
    namespace s   = standalone;

    bool ok = true;
    auto ctx = context::get().init_all([&]([[maybe_unused]] auto& err, [[maybe_unused]] auto& ctx){
        ok = false;
        // TODO: catch init errors
    });
    if(!ok) return 1;

    // TODO: refactor me into file.
    struct object
    {
        using vecf = std::vector<context::vertex_t>;
        using vecidx = std::vector<context::index_t>;
        using f3x32 = std::array<float, 3>; // TODO: override me with custom struct with .xyz, .rgb, etc.
        using on_tick_t = std::function<void(float dt, object& self)>;

        std::string name = "";
        bool draw = true;
        bool tick = true;

        struct mesh_t
        {
            vecf   vertex_data = {};
            vecf   color_data = {};
            vecf   normal_data = {};
            vecidx index_data = {};
        } mesh;

        struct transform
        {
            f3x32 pos   = {0, 0, 0};
            f3x32 rot   = {0, 0, 0}; // In radians.
            f3x32 scale = {1, 1, 1};
        };

        struct constructorargs
        {
            std::string name = "";
            mesh_t mesh = {};
            transform world_transform = {};
            transform model_transform = {};
            on_tick_t on_tick = [](auto, auto){};
            bool draw = true;
            bool tick = true;
        };

        constexpr object(constructorargs&& args)
            : name{ s::move(args.name) }
            , draw{ args.draw }
            , tick{ args.tick }
            , mesh{ s::move(args.mesh) }
            , on_tick{ s::move(args.on_tick) }
            , world_transform{ s::move(args.world_transform) }
            , model_transform{ s::move(args.model_transform) }
        {}

        constexpr auto wt() -> transform& { transform_is_dirty = true; return world_transform; }
        constexpr auto mt() -> transform& { transform_is_dirty = true; return model_transform; }
        constexpr auto wt() const -> transform const& { return world_transform; }
        constexpr auto mt() const -> transform const& { return model_transform; }

        constexpr auto look_at(f3x32 const& target)
        {
            //TODO: Implementar. Só fazer this->mt().rot apontar target - this->mt().pos
        }

        // This is heavy, please only call this if you *really* need to,
        // otherwise this is called for every object at the end of the ticks.
        constexpr auto compute_transform() const -> m4f
        {
            if(!this->transform_is_dirty) return this->cached_transform;
            this->transform_is_dirty = false;

            const auto& wt = this->wt();
            const auto& mt = this->mt();

            return this->cached_transform = m4f
                ::translation(wt.pos[0], wt.pos[1], wt.pos[2])
                .zRotate(wt.rot[2])
                .yRotate(wt.rot[1])
                .xRotate(wt.rot[0])
                .scale(wt.scale[0], wt.scale[1], wt.scale[2])
                .translate(mt.pos[0], mt.pos[1], mt.pos[2])
                .zRotate(mt.rot[2])
                .yRotate(mt.rot[1])
                .xRotate(mt.rot[0])
                .scale(mt.scale[0], mt.scale[1], mt.scale[2]);
        };

        on_tick_t on_tick = [](auto, auto){};

    private:
        transform world_transform = {};
        transform model_transform = {};

        // For optimization purposes.
        bool mutable transform_is_dirty = true;
        m4f  mutable cached_transform   = m4f::scaling(1, 1, 1);
    };

    // TODO: refactor me into file.
    struct userdata
    {
        bool adapter_info_menu_open = false;

        u32 nw = 1280;
        u32 nh = 640;

        bool fixed_tick_rate = true;
        u64 total_ticks = 0;
        u64 fixed_ticks = 0; // Can use as id for lockstep networking or something else.
        float ticks_per_second = 120.0f;
        float leftover_tick_seconds = 0.0f;

        // Perspective transform stuff.
        float focal_len = 0.75;
        float near = 0.01;
        float far  = 100;
        // View transform (camera) stuff + geometry data from
        // all objects that updates every time geometry changes = true.
        object scene = object{{
            .name = "scene",
            .world_transform = { .pos = {0.0, 0.0, 2.0}, .rot = {-3.0f * M_PI / 4.0f, 0.0f, 0.0f} },
            .on_tick = [
                    t = 0.0f,
                    currpoint = 0_u32,
                    path = std::vector<object::f3x32>{
                        {0.0f, 0.0f, 2.0f},
                        {0.5f, 0.5f, 2.0f},
                        {1.0f, 0.5f, 2.0f},
                        {1.0f, 0.0f, 2.0f},
                        {1.0f, -0.5f, 2.0f},
                        {0.5f, -0.5f, 2.0f},
                        {0.0f, -0.5f, 2.0f},
                    }
                ](auto dt, auto& self) mutable
                {
                    t += dt;
                    if(t >= 1)
                    {
                        currpoint += 2; // Porque tem 1 ponto de controle no meio.
                        t -= 1;
                    }

                    const auto p1 = path[(currpoint + 0) % path.size()];
                    const auto p2 = path[(currpoint + 1) % path.size()];
                    const auto p3 = path[(currpoint + 2) % path.size()];

                    // from https://javascript.info/bezier-curve.
                    const auto x = (1-t)*(1-t)*p1[0] + 2*(1-t)*t*p2[0] + t*t*p3[0];
                    const auto y = (1-t)*(1-t)*p1[1] + 2*(1-t)*t*p2[1] + t*t*p3[1];
                    const auto z = (1-t)*(1-t)*p1[2] + 2*(1-t)*t*p2[2] + t*t*p3[2];

                    self.wt().pos[0] = x;
                    self.wt().pos[1] = y;
                    self.wt().pos[2] = z;
                }
        }};
        bool geometry_changed = false;

        float total_seconds = 0.0f;

        std::vector<object> objects;

        constexpr auto tick(auto dt)
        {
            ++ud.total_ticks;

            // TODO: make me parallel by updating against scene snapshots.
            if(ud.scene.tick) ud.scene.on_tick(dt, ud.scene);
            for(auto i = 0_u64; i < ud.objects.size(); ++i)
                if(ud.objects[i].tick) ud.objects[i].on_tick(dt, ud.objects[i]);
        }
    } ud = userdata{};

    ud.geometry_changed = true;

    auto p = object{{
        .mesh = {
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
        }
    }};

    p.name = "Pyramid 2";
    p.wt().pos = {0.5f, 0.0f, 0.0f};
    p.wt().scale = {0.3f, 0.3f, 0.3f};
    p.on_tick = [](auto dt, auto& self) { self.wt().rot[2] += dt; };
    p.draw = false;
    ud.objects.push_back(p);

    p.name = "Pyramid";
    p.wt().pos = {-1.5, 0.0f, -1.0f};
    p.wt().scale = {1.0f, 1.0f, 1.0f};
    p.on_tick = [](auto dt, auto& self) { self.wt().rot[1] += dt; };
    p.draw = true;
    ud.objects.push_back(p);

    ctx.loop(&ud, [](auto dt, auto ctx, auto* _ud)
    {
        auto& ud = *(_ud * cvt::rc<userdata*>);

        ud.total_seconds += dt;

        auto queue = ctx.device.getQueue();

        if(ud.geometry_changed)
        {
            // Rebuild the scene geometry.
            ud.scene.mesh.vertex_data.clear();
            ud.scene.mesh.color_data.clear();
            ud.scene.mesh.normal_data.clear();
            ud.scene.mesh.index_data.clear();
            for(const auto& obj : ud.objects)
            {
                ud.scene.mesh.vertex_data.insert(ud.scene.mesh.vertex_data.end(), obj.mesh.vertex_data.begin(), obj.mesh.vertex_data.end());
                ud.scene.mesh.color_data.insert(ud.scene.mesh.color_data.end(),   obj.mesh.color_data.begin(),  obj.mesh.color_data.end());
                ud.scene.mesh.normal_data.insert(ud.scene.mesh.normal_data.end(), obj.mesh.normal_data.begin(), obj.mesh.normal_data.end());
                ud.scene.mesh.index_data.insert(ud.scene.mesh.index_data.end(),   obj.mesh.index_data.begin(),  obj.mesh.index_data.end());
            }

            // And upload it to the gpu.
            queue.writeBuffer(ctx.vertex_buffer, 0, ud.scene.mesh.vertex_data.data(), ud.scene.mesh.vertex_data.size() * sizeof(context::vertex_t));
            queue.writeBuffer(ctx.color_buffer,  0, ud.scene.mesh.color_data.data(),  ud.scene.mesh.color_data.size()  * sizeof(context::vertex_t));
            queue.writeBuffer(ctx.normal_buffer, 0, ud.scene.mesh.normal_data.data(), ud.scene.mesh.normal_data.size() * sizeof(context::vertex_t));
            queue.writeBuffer(ctx.index_buffer,  0, ud.scene.mesh.index_data.data(),  ud.scene.mesh.index_data.size()  * sizeof(context::index_t));
            ud.geometry_changed = false;
        }

        if(ud.nw != ctx.w || ud.nh != ctx.h) { ctx.set_resolution(ud.nw, ud.nh); }

        // Doing object ticks (fixed tick rate).
        auto ticks = 0_u64; // For debug purposes.
        if(ud.fixed_tick_rate)
        {
            const auto seconds_per_tick = 1.0f / ud.ticks_per_second;
            ud.leftover_tick_seconds += dt;
            while(ud.leftover_tick_seconds >= seconds_per_tick)
            {
                ud.tick(seconds_per_tick);
                ++ticks;
                ++ud.fixed_ticks;
                ud.leftover_tick_seconds -= seconds_per_tick;
            }
        }
        else
        {
            ud.tick(dt + ud.leftover_tick_seconds);
            ud.leftover_tick_seconds = 0.0f;
            ++ticks;
        }

        if(!ticks) continue;

        // Then updating the uniform buffers.
        const auto scene_uniforms = context::scene_uniforms{
            .view       = ud.scene.compute_transform(),
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
        queue.writeBuffer(ctx.scene_uniform_buffer, 0, &scene_uniforms, sizeof(scene_uniforms));

        # pragma omp parallel for
        for(auto i = 0_u64; i < ud.objects.size(); ++i)
        {
            if(!ud.objects[i].draw) continue;

            const auto object_uniform = context::object_uniforms{ .transform = ud.objects[i].compute_transform() };
            queue.writeBuffer(
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
                auto frame_str = ud.fixed_tick_rate ? fmt::format(
                    "{:.1f} FPS ({:.1f}ms) / {} Tris / {} Ticks - {} TPS ({:.1f}ms) / Frame {}",
                    1 / dt, dt * 1000,
                    ud.scene.mesh.index_data.size() / 3,
                    ticks, ud.ticks_per_second, seconds_per_tick * 1000,
                    ctx.frame
                ) : fmt::format(
                    "{:.1f} FPS ({:.1f}ms) / {} Tris / Frame {}",
                    1 / dt, dt * 1000,
                    ud.scene.mesh.index_data.size() / 3,
                    ctx.frame
                );
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(frame_str.c_str()).x -  ImGui::GetStyle().ItemSpacing.x);
                ImGui::Text("%s", frame_str.c_str());
                ImGui::SetCursorPosX(0);

                if(ImGui::BeginMenu("Menu"))
                {
                    ImGui::PushItemWidth(100);
                    ImGui::InputInt("Width",  &ud.nw * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PushItemWidth(100);
                    ImGui::InputInt("Height", &ud.nh * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);

                    ImGui::MenuItem("Show Adapter limits", nullptr, &ud.adapter_info_menu_open);
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMainMenuBar();

            draw_limits_window(ud.adapter_info_menu_open, ctx.limits.adapter, ctx.limits.device);

            if( ImGui::Begin("projection") )
            {
                ImGui::SliderFloat("near", &ud.near, 0.01, 10);
                ImGui::SliderFloat("far", &ud.far, 0, 100);
                ImGui::SliderFloat("focal_len", &ud.focal_len, 0, 10);
                ImGui::SliderFloat3("pos", ud.scene.wt().pos.data(), -1, 1);
                ImGui::SliderFloat3("rot", ud.scene.wt().rot.data(), -3.14, 3.14);
                ImGui::SliderFloat3("sca", ud.scene.wt().scale.data(), 0, 2);

                // TODO: Implement object search window with transform manipulation and mesh preview.
                draw_matrix(scene_uniforms.projection, "Projection", "##projection");
                draw_matrix(scene_uniforms.view, "View", "##view");
                for(auto const& obj : ud.objects)
                    draw_matrix(obj.compute_transform(), obj.name.c_str(), obj.name.c_str());
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
        render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, ud.scene.mesh.vertex_data.size() * sizeof(context::vertex_t));
        render_pass.setVertexBuffer(1, ctx.color_buffer,  0, ud.scene.mesh.color_data.size()  * sizeof(context::vertex_t));
        render_pass.setVertexBuffer(2, ctx.normal_buffer, 0, ud.scene.mesh.normal_data.size() * sizeof(context::vertex_t));
        render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, ud.scene.mesh.index_data.size() * sizeof(context::index_t));
        fmt::print("rendering {}, points = {}, verts = {}\n", ud.scene.name, ud.scene.mesh.vertex_data.size(), ud.scene.mesh.index_data.size());

        auto idx_offset = 0_u32;
        auto vertex_offset = 0_i32;
        auto dynamic_bind_offset = 0_u32;
        for(auto const& obj : ud.objects)
        {
            fmt::print("rendering {}/{}, points/offset = {}/{}, verts/offset = {}/{}\n", obj.name, dynamic_bind_offset, obj.mesh.vertex_data.size(), vertex_offset, obj.mesh.index_data.size(), idx_offset);
            if(obj.draw)
            {
                render_pass.setBindGroup(1, ctx.object_bind_group, 1, &dynamic_bind_offset);
                // TODO: why is the second pyramid drawn wrong ?
                render_pass.drawIndexed(obj.mesh.index_data.size(), 1, idx_offset, vertex_offset, 0);
            }

            idx_offset          += obj.mesh.index_data.size();
            vertex_offset       += obj.mesh.vertex_data.size();
            dynamic_bind_offset += ctx.object_uniform_stride;
        }
        ctx.imgui_render(render_pass);
        render_pass.end();

        auto command = encoder.finish({{
            .nextInChain = nullptr,
            .label = "wgpu-test-command-buffer"
        }});

        queue.submit(command);

        next_texture.drop();
        ctx.swapchain.present();

        return context::loop_message::do_continue;
    });

    return 0;
}

auto draw_limits_window(bool& open, wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void
{
    if(!open) return;

    if(ImGui::Begin("Adapter Limits", &open))
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