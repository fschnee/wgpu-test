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

#include "ghuva/utils.hpp"
#include "ghuva/object.hpp"
#include "ghuva/context.hpp"

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE false
    #define PANIC_ON(cond, msg)
#else
    #define IS_NATIVE true
    #define PANIC_ON(cond, msg) if(cond) { fmt::print("{}", msg); return 1; }
#endif

#define DONT_FORGET(fn) STANDALONE_DONT_FORGET(fn)

auto draw_limits_window(bool& open, wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void;
auto draw_matrix(ghuva::m4f const&, const char*, const char*) -> void;

int main()
{
    using namespace ghuva::integer_aliases;
    namespace cvt = ghuva::cvt;
    namespace g   = ghuva;

    bool ok = true;
    auto ctx = g::context::get().init_all([&]([[maybe_unused]] auto& err, [[maybe_unused]] auto& ctx){
        ok = false;
        // TODO: catch init errors
    });
    if(!ok) return 1;

    // TODO: refactor me into file.
    struct userdata
    {
        bool adapter_info_menu_open = false;

        u32 nw = 1280;
        u32 nh = 640;

        bool fixed_tick_rate = true; // Useful for simulations that require high ticks-per-second for stability
                                     // such as some racing sims, lockstep networking or deterministic physics.
        float ticks_per_second = 120.0f;
        float leftover_tick_seconds = 0.0f;

        u64 fixed_ticks = 0; // Can use as id for lockstep networking or something else.
        u64 total_ticks = 0;

        // Perspective transform stuff.
        float focal_len = 0.75;
        float near = 0.01;
        float far  = 100;
        // View transform (camera) stuff + geometry data from
        // all objects that updates every time geometry changes = true.
        ghuva::object scene = {{
            .name = "scene",
            .world_transform = { .pos = {0.0, 0.0, 2.0}, .rot = {-3.0f * M_PI / 4.0f, 0.0f, 0.0f} },
            .on_tick = [
                    t = 0.0f,
                    currpoint = 0_u32,
                    path = std::vector<ghuva::fpoint>{
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

                    // From https://javascript.info/bezier-curve.
                    self.wt().pos = ghuva::m::satlerp( self.wt().pos, (1-t)*(1-t)*p1 + 2*(1-t)*t*p2 + t*t*p3, dt);
                }
        }};
        bool geometry_changed = false;

        float total_seconds = 0.0f;

        std::vector<ghuva::object> objects;

        constexpr auto tick(float dt)
        {
            ++this->total_ticks;

            // TODO: make me parallel by updating against scene snapshots.
            if(this->scene.tick) this->scene.on_tick(dt, this->scene);
            for(auto i = 0_u64; i < this->objects.size(); ++i)
                if(this->objects[i].tick) this->objects[i].on_tick(dt, this->objects[i]);
        }
    } ud = userdata{};

    ud.geometry_changed = true;

    auto pyramid_mesh = ghuva::object::mesh_t{
        .vertexes = { -0.5, -0.5, -0.3, +0.5, -0.5, -0.3, +0.5, +0.5, -0.3, -0.5, +0.5, -0.3, -0.5, -0.5, -0.3, +0.5, -0.5, -0.3, +0.0, +0.0, +0.5, +0.5, -0.5, -0.3, +0.5, +0.5, -0.3, +0.0, +0.0, +0.5, +0.5, +0.5, -0.3, -0.5, +0.5, -0.3, +0.0, +0.0, +0.5, -0.5, +0.5, -0.3, -0.5, -0.5, -0.3, +0.0, +0.0, +0.5 },
        .colors  = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
        .normals = { 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -0.848, 0.53, 0.0, -0.848, 0.53, 0.0, -0.848, 0.53, 0.848, 0.0, 0.53, 0.848, 0.0, 0.53, 0.848, 0.0, 0.53, 0.0, 0.848, 0.53, 0.0, 0.848, 0.53, 0.0, 0.848, 0.53, -0.848, 0.0, 0.53, -0.848, 0.0, 0.53, -0.848, 0.0, 0.53 },
        .indexes  = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    };

    ud.objects.push_back({{
        .name = "Pyramid",
        .mesh = pyramid_mesh,
        .world_transform = {
            .pos = {-1.5, 0.0f, -1.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        },
        .on_tick = [](auto dt, auto& self) { self.wt().rot.y += dt; },
    }});

    ud.objects.push_back({{
        .name = "Pyramid 2",
        .mesh = pyramid_mesh,
        .world_transform = {
            .pos = {0.5f, 0.0f, 0.0f},
            .scale = {0.3f, 0.3f, 0.3f},
        },
        .on_tick = [](auto dt, auto& self) { self.wt().rot.z += dt; },
    }});

    ud.scene.tick = false;

    ctx.loop(&ud, [](auto dt, auto ctx, auto* _ud)
    {
        auto& ud = *(_ud * cvt::rc<userdata*>);

        ud.total_seconds += dt;

        auto queue = ctx.device.getQueue();

        if(ud.geometry_changed)
        {
            // Rebuild the scene geometry.
            ud.scene.mesh.vertexes.clear();
            ud.scene.mesh.colors.clear();
            ud.scene.mesh.normals.clear();
            ud.scene.mesh.indexes.clear();
            for(const auto& obj : ud.objects)
            {
                ud.scene.mesh.vertexes.insert(ud.scene.mesh.vertexes.end(), obj.mesh.vertexes.begin(), obj.mesh.vertexes.end());
                ud.scene.mesh.colors.insert(ud.scene.mesh.colors.end(),   obj.mesh.colors.begin(),  obj.mesh.colors.end());
                ud.scene.mesh.normals.insert(ud.scene.mesh.normals.end(), obj.mesh.normals.begin(), obj.mesh.normals.end());
                ud.scene.mesh.indexes.insert(ud.scene.mesh.indexes.end(),   obj.mesh.indexes.begin(),  obj.mesh.indexes.end());
            }

            // And upload it to the gpu.
            queue.writeBuffer(ctx.vertex_buffer, 0, ud.scene.mesh.vertexes.data(), ud.scene.mesh.vertexes.size() * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.color_buffer,  0, ud.scene.mesh.colors.data(),  ud.scene.mesh.colors.size()  * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.normal_buffer, 0, ud.scene.mesh.normals.data(), ud.scene.mesh.normals.size() * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.index_buffer,  0, ud.scene.mesh.indexes.data(),  ud.scene.mesh.indexes.size()  * sizeof(g::context::index_t));
            ud.geometry_changed = false;
        }

        if(ud.nw != ctx.w || ud.nh != ctx.h) { ctx.set_resolution(ud.nw, ud.nh); }

        // Doing object ticks.
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

            if(!ticks) return g::context::loop_message::do_continue;
        }
        else
        {
            ud.tick(dt + ud.leftover_tick_seconds);
            ud.leftover_tick_seconds = 0.0f;
            ++ticks;
        }

        // Then updating the uniform buffers.
        const auto scene_uniforms = g::context::scene_uniforms{
            .view       = ud.scene.compute_transform(),
            .projection = ghuva::m4f::perspective({
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

            const auto object_uniform = g::context::object_uniforms{ .transform = ud.objects[i].compute_transform() };
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
                const auto frame_str = ud.fixed_tick_rate ? fmt::format(
                    "{:.1f} FPS ({:.1f}ms) / {} Tris / {} Ticks - {} TPS ({:.1f}ms) / Frame {}",
                    1 / dt, dt * 1000,
                    ud.scene.mesh.indexes.size() / 3,
                    ticks, ud.ticks_per_second, 1 / ud.ticks_per_second * 1000,
                    ctx.frame
                ) : fmt::format(
                    "{:.1f} FPS ({:.1f}ms) / {} Tris / Frame {}",
                    1 / dt, dt * 1000,
                    ud.scene.mesh.indexes.size() / 3,
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
        if(!next_texture) return g::context::loop_message::do_break;

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
        render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, ud.scene.mesh.vertexes.size() * sizeof(g::context::vertex_t));
        render_pass.setVertexBuffer(1, ctx.color_buffer,  0, ud.scene.mesh.colors.size()   * sizeof(g::context::vertex_t));
        render_pass.setVertexBuffer(2, ctx.normal_buffer, 0, ud.scene.mesh.normals.size()  * sizeof(g::context::vertex_t));
        render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, ud.scene.mesh.indexes.size() * sizeof(g::context::index_t));

        auto first_index = 0_u32;
        auto base_vertex = 0_i32;
        auto dynamic_bind_offset = 0_u32;
        for(auto const& obj : ud.objects)
        {
            if(obj.draw)
            {
                render_pass.setBindGroup(1, ctx.object_bind_group, 1, &dynamic_bind_offset);
                render_pass.drawIndexed(obj.mesh.indexes.size(), 1, first_index, base_vertex, 0);
            }

            first_index         += obj.mesh.indexes.size();
            base_vertex         += obj.mesh.vertexes.size() / 3; // Divide by 3 since each vertex has x, y, z.
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

        return g::context::loop_message::do_continue;
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
                ghuva::clean_type_name(decltype(wgpu::Limits::name){}), \
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

auto draw_matrix(ghuva::m4f const& m, const char* panelname, const char* tablename) -> void
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