#include "app.hpp"

#include <algorithm> // std::find_if.
#include <cstring> // std::memcpy.

#include "ghuva/utils/list.hpp"
#include "ghuva/utils.hpp"
#include "ghuva/mesh.hpp"

#include <imgui.h>
#include "imgui_widgets/group_panel.hpp"

#include <fmt/core.h>

using namespace ghuva::aliases;
namespace cvt = ghuva::cvt;

app::app()
    : ctx{ ghuva::context::get() }
{}

app::~app()
{
    if(scene.geometry_buffer.data != nullptr) delete scene.geometry_buffer.data;
    if(scene.instance_buffer.data != nullptr) delete scene.instance_buffer.data;
    if(scene.index_buffer.data    != nullptr) delete scene.index_buffer.data;
}

auto app::init() -> void
{
    ctx.init_all();
}

auto app::loop(void* original_userdata, loop_callback user_callback) -> void
{
    struct userdata
    {
        app* me;
        void* original_userdata;
        loop_callback user_callback;
    };

    auto ud = userdata{
        .me = this,
        .original_userdata = original_userdata,
        .user_callback = user_callback
    };

    ctx.loop(&ud, []([[maybe_unused]] ghuva::context& ctx, f32 dt, void* _ud) -> ghuva::context::loop_message
    {
        auto& ud = *(_ud * cvt::rc<userdata*>);
        auto& app = *ud.me;

        auto ret = ud.user_callback(app, dt, ud.original_userdata);
        if(ret != ghuva::context::loop_message::do_continue) return ret;
        return app.loop_impl(dt);
    });
}

auto app::loop_impl(f32 dt) -> ghuva::context::loop_message
{
    if(ui.w != ctx.w || ui.h != ctx.h) ctx.set_resolution(ui.w, ui.h);

    sync_params_to_outputs();
    write_scene_uniform();
    build_scene_geometry();
    write_geometry_buffers();
    do_ui(dt);
    if(compute_pass) compute_transform_matrix_via_compute_pass();

    return render();
}

auto app::sync_params_to_outputs() -> void
{
    outputs.do_engine_config_update = false;
    outputs.target_tps              = params.engine.tps;
    outputs.target_time_multiplier  = params.engine.time_multiplier;
    outputs.parallel_ticking        = params.engine.parallel_ticking;
}

auto app::write_scene_uniform() -> void
{
    // Then updating the uniform buffers.
    ui.scene_uniforms = {
        .view = ghuva::m4f::from_parts(params.camera.t.pos, params.camera.t.rot, params.camera.t.scale),
        .projection = ghuva::m4f::perspective({
            .focal_len    = params.camera.focal_len,
            .aspect_ratio = (ui.w * cvt::to<f32>) / (ui.h * cvt::to<f32>),
            .near         = params.camera.near_plane,
            .far          = params.camera.far_plane,
        }),

        .light_direction = {0.2f, 0.4f, 0.3f},
        .light_color     = {1.0f, 0.9f, 0.6f},

        .time  = params.engine.total_time,
        .gamma = 2.2,
    };
    ctx.device.getQueue().writeBuffer(ctx.scene_uniform_buffer, 0, &ui.scene_uniforms, sizeof(ui.scene_uniforms));
}

auto app::build_scene_geometry() -> void
{
    if(params.meshes == nullptr || params.mesh_count == 0_u64) return;

    scene.geometry_offsets.clear();
    scene.geometry_offsets.reserve(params.mesh_count);

    // Prepare for instancing.
    std::sort(
        params.meshes,
        params.meshes + params.mesh_count,
        [](auto a, auto b){ return a.id < b.id; }
    ); // Sort by id ASC.

    // Build the geometry_offsets first so we can do the buffer allocation all at once using the sizes found.
    auto curr_idx    = 0_u64;
    auto curr_vertex = 0_u64;
    for(auto i = 0_u64; i < params.mesh_count; ++i)
    {
        auto& mesh = params.meshes[i];

        scene.geometry_offsets.push_back({
            .id = mesh.id,
            .instance_count = 0,
            .start_index = curr_idx,
            .index_count = mesh.indexes.size(),
            .start_vertex = curr_vertex,
        });

        curr_idx    += mesh.indexes.size();
        curr_vertex += mesh.vertexes.size() / 3;
    }

    scene.index_buffer = ghuva::list<ghuva::context::index_t>
        ::from_container( ghuva::move(scene.index_buffer) )
        .reserve_nocopy(curr_idx)
        .override_size(curr_idx)
        .surrender();

    auto const vertex_count = curr_vertex * 3; // * 3 since each buffer has 3 elements per vertex
                                               // (pos has x,y,z; color has r,g,b; normal has nx,ny,nz).
    auto const geometry_buf_size = vertex_count * 3; // * 3 since there are 3 buffers (position, color, normals).
    scene.geometry_buffer = ghuva::list<ghuva::context::vertex_t>
        ::from_container( ghuva::move(scene.geometry_buffer) )
        .reserve_nocopy(geometry_buf_size)
        .override_size(geometry_buf_size)
        .surrender();

    // Copy all the mesh data into our buffers.
    auto const position_start = scene.geometry_buffer.data;
    auto const color_start    = position_start + scene.geometry_buffer.size / 3;
    auto const normal_start   = color_start    + scene.geometry_buffer.size / 3;
    auto const index_start    = scene.index_buffer.data;

    auto vertex_offset = 0_u64;
    auto index_offset  = 0_u64;
    for(auto i = 0_u64; i < params.mesh_count; ++i)
    {
        auto const& mesh = params.meshes[i];

        auto const vertex_bsize = mesh.vertexes.size() * sizeof(ghuva::context::vertex_t);
        auto const index_bsize  = mesh.indexes.size()  * sizeof(ghuva::context::index_t);

        std::memcpy(position_start + vertex_offset, mesh.vertexes.data(), vertex_bsize);
        std::memcpy(color_start    + vertex_offset, mesh.colors.data(),   vertex_bsize);
        std::memcpy(normal_start   + vertex_offset, mesh.normals.data(),  vertex_bsize);
        std::memcpy(index_start    + index_offset,  mesh.indexes.data(),  index_bsize);

        vertex_offset += mesh.vertexes.size();
        index_offset  += mesh.indexes.size();
    }

    // And finally, build our instancing buffer.
    if(params.object_count == 0) return;

    scene.instance_buffer = ghuva::list<ghuva::context::object_uniforms>
        ::from_container( ghuva::move(scene.instance_buffer) )
        .reserve_nocopy(params.object_count)
        .override_size(params.object_count)
        .surrender();

    std::sort(
        params.objects,
        params.objects + params.object_count,
        [](auto a, auto b){ return a.mesh_id < b.mesh_id; }
    ); // Sort by mesh_id ASC.

    auto last_mesh_id    = params.objects[0].mesh_id;
    auto last_mesh_index = 0_u64;
    for(auto i = 0_u64; i < params.object_count; ++i)
    {
        auto& obj = params.objects[i];
        if(obj.mesh_id != last_mesh_id) ++last_mesh_index;

        ++scene.geometry_offsets[last_mesh_index].instance_count;

        auto const offset = scene.instance_buffer.data + i;
        if(compute_pass)
        {
            new (offset) ghuva::context::compute_object_uniforms{
                .pos   = {obj.t.pos.x,   obj.t.pos.y,   obj.t.pos.z},
                .rot   = {obj.t.rot.x,   obj.t.rot.y,   obj.t.rot.z},
                .scale = {obj.t.scale.x, obj.t.scale.y, obj.t.scale.z},
            };
        }
        else
        {
            new (offset) ghuva::context::object_uniforms{ ghuva::m4f::from_parts(
                obj.t.pos,
                obj.t.rot,
                obj.t.scale
            )};
        }

        last_mesh_id = obj.mesh_id;
    }
}

auto app::write_geometry_buffers() -> void
{
    if(scene.geometry_buffer.data == nullptr || scene.geometry_buffer.size == 0) return;

    auto const position_start = scene.geometry_buffer.data;
    auto const color_start    = position_start + scene.geometry_buffer.size / 3;
    auto const normal_start   = color_start    + scene.geometry_buffer.size / 3;
    auto const index_start    = scene.index_buffer.data;

    ctx.device.getQueue().writeBuffer(ctx.vertex_buffer, 0, position_start,  scene.geometry_buffer.byte_size() / 3);
    ctx.device.getQueue().writeBuffer(ctx.color_buffer,  0, color_start,     scene.geometry_buffer.byte_size() / 3);
    ctx.device.getQueue().writeBuffer(ctx.normal_buffer, 0, normal_start,    scene.geometry_buffer.byte_size() / 3);
    ctx.device.getQueue().writeBuffer(ctx.index_buffer,  0, index_start,     scene.index_buffer.byte_size());

    if(scene.instance_buffer.data == nullptr || scene.instance_buffer.size == 0) return;
    ctx.device.getQueue().writeBuffer(ctx.object_uniform_buffer, 0, scene.instance_buffer.data, scene.instance_buffer.byte_size());
}

auto app::do_ui(f32 dt) -> void
{
    ctx.imgui_new_frame();

    ImGui::BeginMainMenuBar();
    {
        auto const frame_str = fmt::format(
            "{:.1f} FPS ({:.1f}ms) / Scene buffers: G({}b/{}b) In({}b/{}b) Idx({}b/{}b) / {} Renderables / {} Ticks - {} TPS ({:.1f}ms) / Frame {}",
            1 / dt, dt * 1000,
            scene.geometry_buffer.byte_size(), scene.geometry_buffer.byte_capacity(),
            scene.index_buffer.byte_size(),    scene.index_buffer.byte_capacity(),
            scene.instance_buffer.byte_size(), scene.instance_buffer.byte_capacity(),
            params.object_count,
            params.engine.ticks, params.engine.tps, 1 / params.engine.tps * 1000,
            ctx.frame
        );
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - ImGui::CalcTextSize(frame_str.c_str()).x -  ImGui::GetStyle().ItemSpacing.x);
        ImGui::Text("%s", frame_str.c_str());
        ImGui::SetCursorPosX(0);

        if(ImGui::BeginMenu("Options"))
        {
            ImGui::PushItemWidth(100);
            ImGui::InputInt("Width",  &ui.w * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PushItemWidth(100);
            ImGui::InputInt("Height", &ui.h * cvt::rc<int*>, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::NewLine();

            ImGui::PushItemWidth(100);
            if(ImGui::Checkbox("Maintain TPS ratio", &ui.fixed_ticks_per_frame))
            {
                outputs.do_engine_config_update = true;

                if(ui.fixed_ticks_per_frame)
                {
                    ui.cached_target_tps = params.engine.tps; // When becomes true, we cache the old one.
                    outputs.target_tps = ghuva::m::clamp(
                        outputs.target_tps * 1.0f / outputs.target_time_multiplier,
                        0.0f,
                        params.engine.max_tps
                    );
                }
                else { outputs.target_tps = ui.cached_target_tps; } // And when it turns false we restore it.
            }
            ImGui::PushItemWidth(100);
            if(ImGui::InputFloat("TMult", &outputs.target_time_multiplier, 0.1f, 0.5f))
            {
                outputs.do_engine_config_update = true;

                // TODO: fixme! If we set time_multiplier to 0 the engine stalls.
                outputs.target_time_multiplier = outputs.target_time_multiplier < 0.0f ? 0.0001f : outputs.target_time_multiplier;

                if(ui.fixed_ticks_per_frame)
                    outputs.target_tps = ghuva::m::clamp(
                        outputs.target_time_multiplier == 0.0f
                            ? 0.0f
                            : outputs.target_tps * 1.0f / outputs.target_time_multiplier,
                        0.0f,
                        params.engine.max_tps);
            }
            ImGui::BeginDisabled(ui.fixed_ticks_per_frame);
            ImGui::PushItemWidth(100);
            if(ImGui::InputFloat("TPS", &outputs.target_tps, 10.0f, 100.0f))
            {
                outputs.do_engine_config_update = true;
                outputs.target_tps = ghuva::m::clamp(outputs.target_tps, 0.0f, params.engine.max_tps);
            }
            ImGui::EndDisabled();

            ImGui::NewLine();

            ImGui::Checkbox("Compute pass", &compute_pass);
            ImGui::SameLine();
            ui_help("Whether or not to compute the per-object transformation matrixes using a compute pass instead of doing it on the CPU.\n\nFor scenes with a lot of objects, this *should* improve performance");

            ImGui::Checkbox("Engine Thread", &outputs.engine_has_dedicated_thread);
            ImGui::SameLine();
            ui_help("Whether or not to run the engine on a dedicated thread separate of the render thread");

            if(ImGui::Checkbox("Parallel ticking", &outputs.parallel_ticking))
            { outputs.do_engine_config_update = true; }
            ImGui::SameLine();
            ui_help("Whether or not to use multiple threads to do object ticking");

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Projection",     nullptr, &ui.window.projection);
            ImGui::MenuItem("Adapter limits", nullptr, &ui.window.adapter_info);
            ImGui::MenuItem("ImGui Demo",     nullptr, &ui.window.imgui_demo);
            ImGui::EndMenu();
        }
    }
    ImGui::EndMainMenuBar();

    ui_draw_projection_window();
    ui_draw_limits_window();
    if(ui.window.imgui_demo) ImGui::ShowDemoWindow(&ui.window.imgui_demo);
    // TODO: ui_draw_timings_window();
    // TODO: Implement object search window with transform manipulation and mesh preview.
}

// 'Borrowed' directly from imgui_demo.cpp.
// Helper to display a little (?) mark which shows a tooltip when hovered.
auto app::ui_help(const char* desc) -> void
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

auto app::ui_draw_projection_window() -> void
{
    if(!ui.window.projection) return;

    if(ImGui::Begin("Projection", &ui.window.projection))
    {
        ImGui::SliderFloat("near_plane", &params.camera.near_plane, 0.01f, 10.f);
        ImGui::SliderFloat("far_plane",  &params.camera.far_plane,  0.f,   100.f);
        ImGui::SliderFloat("focal_len",  &params.camera.focal_len,  0.f,   10.f);

        ui_draw_matrix(ui.scene_uniforms.projection, "Projection", "##projection");
        ui_draw_matrix(ui.scene_uniforms.view,       "View",       "##view");
    }
    ImGui::End();
}

auto app::ui_draw_limits_window() -> void
{
    if(!ui.window.adapter_info) return;

    if(ImGui::Begin("Adapter Limits", &ui.window.adapter_info))
    {
        #define STRINGIFY(a) STRINGIFY_IMPL(a)
        #define STRINGIFY_IMPL(a) #a
        #define IMGUI_LIMIT_DISPLAY(name)                                    \
            ImGui::Text(fmt::format(                                         \
                "{:<41}: {} = {:^20} {:^20}",                                \
                STRINGIFY(name),                                             \
                ghuva::clean_type_name(decltype(wgpu::Limits::name){}),      \
                ctx.limits.adapter.limits.name,                              \
                ctx.limits.device.limits.name                                \
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

auto app::ui_draw_matrix(ghuva::m4f const& m, const char* panelname, const char* tablename) -> void
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

auto app::compute_transform_matrix_via_compute_pass() -> void
{
    if(scene.instance_buffer.data == nullptr || scene.instance_buffer.size == 0) return;

    auto compute_pass = ctx.begin_compute();

    compute_pass.setPipeline(ctx.compute_pipeline);
    compute_pass.setBindGroup(0, ctx.compute_bind_group, 0, nullptr);
    auto const workgroup_size = 64; // Defined in the shader.
    auto const dispatched     = (params.object_count + workgroup_size - 1) / workgroup_size; // Round up.
    compute_pass.dispatchWorkgroups(dispatched, 1, 1);

    ctx.end_compute(compute_pass);
}

auto app::render() -> ghuva::context::loop_message
{
    auto next_texture = ctx.swapchain.getCurrentTextureView();
    if(!next_texture) return ghuva::context::loop_message::do_break;

    auto render_pass = ctx.begin_render(next_texture);
    render_emit_draw_calls(render_pass);
    ctx.end_render(render_pass);
    next_texture.drop();

    return ghuva::context::loop_message::do_continue;
}

auto app::render_emit_draw_calls(wgpu::RenderPassEncoder render_pass) -> void
{
    if(scene.geometry_buffer.data == nullptr || scene.geometry_buffer.size == 0) return;
    if(scene.instance_buffer.data == nullptr || scene.instance_buffer.size == 0) return;
    if(scene.index_buffer.data == nullptr    || scene.index_buffer.size == 0)    return;

    render_pass.setPipeline(ctx.pipeline);
    render_pass.setBindGroup(0, ctx.scene_bind_group, 0, nullptr);
    render_pass.setVertexBuffer(0, ctx.vertex_buffer, 0, scene.geometry_buffer.byte_size() / 3);
    render_pass.setVertexBuffer(1, ctx.color_buffer,  0, scene.geometry_buffer.byte_size() / 3);
    render_pass.setVertexBuffer(2, ctx.normal_buffer, 0, scene.geometry_buffer.byte_size() / 3);
    render_pass.setVertexBuffer(3, ctx.object_uniform_buffer, 0, scene.instance_buffer.byte_size());
    render_pass.setIndexBuffer(ctx.index_buffer, wgpu::IndexFormat::Uint16, 0, scene.index_buffer.byte_size());

    auto curr_instance = 0_u32;
    for(auto m : scene.geometry_offsets)
    {
        if(m.instance_count == 0) continue;

        render_pass.drawIndexed(
            m.index_count,
            m.instance_count,
            m.start_index,
            m.start_vertex,
            curr_instance
        );

        curr_instance += m.instance_count;
    }
}
