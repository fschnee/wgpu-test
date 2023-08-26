#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>

#include <thread>
#include <atomic>
#include <omp.h>

// Unused for now.
//#include <rapidobj/rapidobj.hpp>
//#include "zep.hpp"

#include "webgpu.hpp"

#include <imgui.h>
#include "imgui_widgets/group_panel.hpp"

#include "ghuva/context.hpp"
#include "ghuva/object.hpp"
#include "ghuva/engine.hpp"
#include "ghuva/utils.hpp"
#include "ghuva/mesh.hpp"
#include "ghuva/objects/eel.hpp"
#include "ghuva/objects/ew.hpp"
#include "ghuva/utils/math.hpp"

using namespace ghuva::aliases;
namespace cvt = ghuva::cvt;
namespace g   = ghuva;

auto load_scene(g::default_engine& engine) -> void;
auto draw_limits_window(bool& open, wgpu::SupportedLimits& adapter,  wgpu::SupportedLimits& device) -> void;
auto draw_matrix(g::m4f const&, const char*, const char*) -> void;

int main()
{
    bool ok = true;
    auto ctx = g::context::get().init_all([&]([[maybe_unused]] auto& err, [[maybe_unused]] auto& ctx){
        ok = false;
        // TODO: catch init errors
    });
    if(!ok) return 1;

    // TODO: refactor me out of here.
    struct userdata
    {
        bool adapter_info_menu_open = false;

        u32 nw = 1280;
        u32 nh = 640;

        g::default_engine engine = g::default_engine{};
        std::atomic_bool exit = false; // Flag to exit the engine_thread.
        std::jthread engine_thread = std::jthread{
            [this, stopwatch = ghuva::chrono::stopwatch{}]() mutable
            {
                while(!this->exit)
                    this->engine.tick(stopwatch.click().last_segment());
            }
        };

        u64 last_snapshot_tick = 0;

        // TODO: move me to camera object ?
        // Perspective transform stuff.
        f32 focal_len = 0.75;
        f32 near = 0.01;
        f32 far  = 100;

        // TODO: Make time_multiplier part of engine.
        f32 time_multiplier = 1.0f;
        f32 target_tps = 120.0f;
        bool fixed_ticks_per_frame = false;
        f32 max_tps = 10'000.0f;
    } ud = userdata{};
    using engine_t = g::remove_cvref_t< decltype(ud.engine) >;

    load_scene(ud.engine);

    ctx.loop(&ud, [](auto dt, auto ctx, auto* _ud)
    {
        auto& ud     = *(_ud * cvt::rc<userdata*>);
        auto  queue  = ctx.device.getQueue();

        // TODO: decouple engine and render threads entirely.
        auto const snapshot = ud.engine.take_snapshot(); // Copy.
        auto const ticks = snapshot.id - ud.last_snapshot_tick;
        ud.last_snapshot_tick = snapshot.id;
        // Uncomment to not update if there were no ticks.
        // if(ticks == 0) return g::context::loop_message::do_continue;

        if(ud.nw != ctx.w || ud.nh != ctx.h) { ctx.set_resolution(ud.nw, ud.nh); }

        auto camera = std::find_if(snapshot.objects.begin(), snapshot.objects.end(), [&](auto const& e){
            return e.id == snapshot.camera_object_id;
        });

        // Then updating the uniform buffers.
        auto const scene_uniforms = g::context::scene_uniforms{
            .view = camera != snapshot.objects.end()
                ? g::m4f::from_parts(camera->t.pos, camera->t.rot, camera->t.scale)
                : g::m4f::scaling(1, 1, 1),
            .projection = g::m4f::perspective({
                .focal_len    = ud.focal_len,
                .aspect_ratio = (ud.nw * cvt::to<float>) / (ud.nh * cvt::to<float>),
                .near         = ud.near,
                .far          = ud.far
            }),

            .light_direction = {0.2f, 0.4f, 0.3f},
            .light_color     = {1.0f, 0.9f, 0.6f},

            .time  = snapshot.engine_config.total_dt,
            .gamma = 2.2,
        };
        queue.writeBuffer(ctx.scene_uniform_buffer, 0, &scene_uniforms, sizeof(scene_uniforms));

        struct mesh_data
        {
            u64 id;
            u64 start_index;
            u64 index_count;
            u64 start_vertex;
        };
        std::vector<mesh_data> mesh_infos; // Used to lookup buffer offsets when rendering.
        auto render_data = g::mesh{};
        auto curr_idx    = 0_u64;
        auto curr_vertex = 0_u64;
        for(auto& mesh : ud.engine.meshes)
        {
            mesh_infos.push_back({
                .id = mesh.id,
                .start_index = curr_idx,
                .index_count = mesh.indexes.size(),
                .start_vertex = curr_vertex,
            });
            render_data.vertexes.insert(render_data.vertexes.end(), mesh.vertexes.begin(), mesh.vertexes.end());
            render_data.colors.insert(render_data.colors.end(),     mesh.colors.begin(), mesh.colors.end());
            render_data.normals.insert(render_data.normals.end(),   mesh.normals.begin(), mesh.normals.end());
            render_data.indexes.insert(render_data.indexes.end(),   mesh.indexes.begin(), mesh.indexes.end());
            curr_idx    += mesh.indexes.size();
            curr_vertex += mesh.vertexes.size() / 3;
        }
        // Upload render_data to the gpu.
        if(render_data.vertexes.size())
        {
            queue.writeBuffer(ctx.vertex_buffer, 0, render_data.vertexes.data(), render_data.vertexes.size() * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.color_buffer,  0, render_data.colors.data(),   render_data.colors.size()   * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.normal_buffer, 0, render_data.normals.data(),  render_data.normals.size()  * sizeof(g::context::vertex_t));
            queue.writeBuffer(ctx.index_buffer,  0, render_data.indexes.data(),  render_data.indexes.size()  * sizeof(g::context::index_t));
        }

        // TODO: maybe on large amounts of objects offload this to a compute pass.
        auto object_uniform_offset = 0_u64;
        for(auto& obj : snapshot.objects)
        {
            if(!obj.draw || obj.mesh_id == 0) continue;

            auto const object_uniform = g::context::object_uniforms{
                .transform = g::m4f::from_parts(obj.t.pos, obj.t.rot, obj.t.scale)
            };
            queue.writeBuffer(
                ctx.object_uniform_buffer,
                object_uniform_offset * ctx.object_uniform_stride,
                &object_uniform,
                sizeof(object_uniform)
            );
            ++object_uniform_offset;
        }

        // UI stuffs.
        ctx.imgui_new_frame();
        { // Scope only for IDE collapsing purposes.
            ImGui::BeginMainMenuBar();
            {
                auto const frame_str =  fmt::format(
                    "{:.1f} FPS ({:.1f}ms) / {} Tris Loaded / {} Ticks - {} TPS ({:.1f}ms) / Frame {}",
                    1 / dt, dt * 1000,
                    render_data.indexes.size() / 3,
                    ticks, snapshot.engine_config.ticks_per_second, 1 / snapshot.engine_config.ticks_per_second * 1000,
                    ctx.frame
                );
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(frame_str.c_str()).x -  ImGui::GetStyle().ItemSpacing.x);
                ImGui::Text("%s", frame_str.c_str());
                ImGui::SetCursorPosX(0);

                if(ImGui::BeginMenu("Menu"))
                {
                    ImGui::PushItemWidth(150);
                    ImGui::InputInt("Width",  &ud.nw * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PushItemWidth(150);
                    ImGui::InputInt("Height", &ud.nh * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);

                    ImGui::NewLine();
                    ImGui::MenuItem("Show Adapter limits", nullptr, &ud.adapter_info_menu_open);
                    ImGui::NewLine();

                    ImGui::PushItemWidth(150);

                    if(ImGui::Checkbox("Fixed Ticks per Frame", &ud.fixed_ticks_per_frame))
                    {
                        if(ud.fixed_ticks_per_frame)
                        {
                            ud.engine.post(engine_t::change_tps{
                                .tps = g::m::clamp(ud.target_tps * 1.0f / ud.time_multiplier, 0.0f, ud.max_tps)
                            });
                        }
                        else
                        { ud.engine.post(engine_t::change_tps{ .tps = ud.target_tps }); }
                    }
                    ImGui::PushItemWidth(100);
                    if(ImGui::InputFloat("Time multiplier", &ud.time_multiplier, 0.1f, 0.5f))
                    {
                        ud.time_multiplier = ud.time_multiplier < 0.0f ? 0.0f : ud.time_multiplier;

                        if(ud.fixed_ticks_per_frame)
                            ud.engine.post(engine_t::change_tps{
                                .tps = g::m::clamp(
                                    ud.time_multiplier <= 0.0f
                                        ? ud.time_multiplier
                                        : ud.target_tps * 1.0f / ud.time_multiplier,
                                    0.0f,
                                    ud.max_tps)
                            });
                    }
                    ImGui::BeginDisabled(ud.fixed_ticks_per_frame);
                    ImGui::PushItemWidth(100);
                    if(ImGui::InputFloat("TPS", &ud.target_tps, 10.0f, 100.0f))
                        ud.engine.post(engine_t::change_tps{ .tps = g::m::clamp(ud.target_tps, 0.0f, ud.max_tps) });
                    ImGui::EndDisabled();

                    ImGui::EndMenu();
                }
            }
            ImGui::EndMainMenuBar();

            draw_limits_window(ud.adapter_info_menu_open, ctx.limits.adapter, ctx.limits.device);

            if( ImGui::Begin("Projection") )
            {
                ImGui::SliderFloat("near", &ud.near, 0.01, 10);
                ImGui::SliderFloat("far", &ud.far, 0, 100);
                ImGui::SliderFloat("focal_len", &ud.focal_len, 0, 10);

                // TODO: Implement dynamic_bind_offsetobject search window with transform manipulation and mesh preview.
                draw_matrix(scene_uniforms.projection, "Projection", "##projection");
                draw_matrix(scene_uniforms.view, "View", "##view");
                for(auto const& obj : snapshot.objects)
                {
                    draw_matrix(
                        g::m4f::from_parts(obj.t.pos, obj.t.rot, obj.t.scale),
                        obj.name.c_str(),
                        obj.name.c_str()
                    );
                }
            }
            ImGui::End();
        }

        // Frame rendering stuff.

        auto next_texture = ctx.swapchain.getCurrentTextureView();
        if(!next_texture) return g::context::loop_message::do_break;

        auto render_pass = ctx.begin_render(next_texture);
        render_pass.setPipeline(ctx.pipeline);
        render_pass.setBindGroup(0, ctx.scene_bind_group, 0, nullptr);
        if(render_data.vertexes.size())
        {
            render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, render_data.vertexes.size() * sizeof(g::context::vertex_t));
            render_pass.setVertexBuffer(1, ctx.color_buffer,  0, render_data.colors.size()   * sizeof(g::context::vertex_t));
            render_pass.setVertexBuffer(2, ctx.normal_buffer, 0, render_data.normals.size()  * sizeof(g::context::vertex_t));
            render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, render_data.indexes.size() * sizeof(g::context::index_t));
        }

        auto dynamic_bind_offset = 0_u32;
        // TODO: (maybe ?) handle instanced meshes/objects somehow.
        for(auto const& obj : snapshot.objects)
        {
            if(!obj.draw || obj.mesh_id == 0) continue;

            auto mesh_info = std::find_if(mesh_infos.begin(), mesh_infos.end(), [&](auto& m){ return m.id == obj.mesh_id; });
            if(mesh_info != mesh_infos.end())
            {
                render_pass.setBindGroup(1, ctx.object_bind_group, 1, &dynamic_bind_offset);
                render_pass.drawIndexed(mesh_info->index_count, 1, mesh_info->start_index, mesh_info->start_vertex, 0);
            }
            else
            {
                fmt::print("Error: trying to render unloaded mesh (mesh_id = {}, objid = {})\n", obj.mesh_id, obj.id);
            }
            dynamic_bind_offset += ctx.object_uniform_stride;
        }
        ctx.end_render(render_pass);
        next_texture.drop();

        return g::context::loop_message::do_continue;
    });

    ud.exit = true; // So the engine thread also stops.

    return 0;
}

auto load_scene(g::default_engine& engine) -> void
{
    using engine_t = g::remove_cvref_t< decltype(engine) >;

    auto const eel_post_id = engine.post(engine_t::register_object{ g::objects::make_eel<engine_t>() });
    fmt::print("[main.load_scene] Requested engine to register EEL {{ .event_id = {} }}\n", eel_post_id);

    auto const camera_post_id = engine.post(engine_t::register_object{ .object = {{
        .name = "Camera",
        .t = {
            .pos = {0.0, 0.0, 2.0},
            .rot = {-3.0f * M_PI / 4.0f, 0.0f, 0.0f},
        },
        .on_tick = [](auto& self, auto, auto const& snapshot, auto& engine)
        {
            if(snapshot.camera_object_id != self.id)
                engine.post(engine_t::set_camera{ .object_id = self.id });
        }
    }}});
    fmt::print("[main.load_scene] Requested engine to register Camera {{ .event_id = {} }}\n", camera_post_id);

    auto const pyramid_mesh_post_id = engine.post(engine_t::register_mesh{ .mesh = {
        .id = 0,
        .vertexes = { -0.5, -0.5, -0.3, +0.5, -0.5, -0.3, +0.5, +0.5, -0.3, -0.5, +0.5, -0.3, -0.5, -0.5, -0.3, +0.5, -0.5, -0.3, +0.0, +0.0, +0.5, +0.5, -0.5, -0.3, +0.5, +0.5, -0.3, +0.0, +0.0, +0.5, +0.5, +0.5, -0.3, -0.5, +0.5, -0.3, +0.0, +0.0, +0.5, -0.5, +0.5, -0.3, -0.5, -0.5, -0.3, +0.0, +0.0, +0.5 },
        .colors  = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
        .normals = { 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0, -0.848, 0.53, 0.0, -0.848, 0.53, 0.0, -0.848, 0.53, 0.848, 0.0, 0.53, 0.848, 0.0, 0.53, 0.848, 0.0, 0.53, 0.0, 0.848, 0.53, 0.0, 0.848, 0.53, 0.0, 0.848, 0.53, -0.848, 0.0, 0.53, -0.848, 0.0, 0.53, -0.848, 0.0, 0.53 },
        .indexes  = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    }});
    fmt::print("[main.load_scene] Requested engine to register pyramid_mesh {{ .event_id = {} }}\n", pyramid_mesh_post_id);

    // Our pyramid loader will wait for the mesh to be registered to get it's id.
    auto const ew_post_id = engine.post(engine_t::register_object{ g::objects::make_ew<engine_t>(pyramid_mesh_post_id, [](
        auto const& _e, auto, auto const& snapshot, auto& engine
    ){
        // Due to EW checking all event types we need to tell the compiler exactly what kind of event this is.
        // In this situation we know the event type (posted it just above), but in the middle of game code
        // it could get get messy and easy to make mistakes. Don't blindly cast like I do here.
        // TODO: EW for single event type -> add template param.
        auto const& e = _e * cvt::rc<engine_t::e_register_mesh const&>;
        auto const mesh_id = e.body.mesh.id;
        auto const p1eid = engine.post(engine_t::register_object{ .object = {{
            .name = "Pyramid",
            .t = { .pos = {-1.5, 0.0f, -1.0f} },
            .on_tick = [](auto& self, auto dt, auto const&, auto&) { self.t.rot.y += dt; },
            .mesh_id = mesh_id,
        }}});
        fmt::print("[ghuva::engine/t{}][main.load_scene] Requested engine to register Pyramid {{ .event_id={} }}\n", snapshot.id, p1eid);
        auto const p2eid = engine.post(engine_t::register_object{ .object = {{
            .name = "Pyramid 2",
            .t = {
                .pos = {0.5f, 0.0f, 0.0f},
                .scale = {0.3f, 0.3f, 0.3f},
            },
            .on_tick = [](auto& self, auto dt, auto const&, auto&) { self.t.rot.z += dt; },
            .mesh_id = mesh_id,
        }}});
        fmt::print("[ghuva::engine/t{}][main.load_scene] Requested engine to register Pyramid 2 {{ .event_id={} }}\n", snapshot.id, p2eid);
    })});
    fmt::print("[main.load_scene] Requested engine to register EW for {{ .event_id = {} }}\n", ew_post_id);
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
                g::clean_type_name(decltype(wgpu::Limits::name){}), \
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

auto draw_matrix(g::m4f const& m, const char* panelname, const char* tablename) -> void
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