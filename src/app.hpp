// Should be complately decoupled from engine, pass relevant info through main().
#pragma once

#include <vector>

#include "ghuva/context.hpp"

#include "ghuva/utils/container.hpp"
#include "ghuva/utils/aliases.hpp"
#include "ghuva/utils/point.hpp"
#include "ghuva/transform.hpp"

// Forward decl.
namespace ghuva{ struct mesh; }

struct app
{
    struct object /* this is app::object, not to be confused with ghuva::object */
    {
        ghuva::u64 mesh_id;
        ghuva::transform t;
        ghuva::u64 mesh_index; // Maintained by the app, don't worry about it.
    };

    struct /* params */ // Set these from your own callback during loop.
    {
        struct /* engine */
        {
            // Engine config stuff.
            ghuva::f32 total_time         = 0.f; // Can be different due to time dilation.
            ghuva::f32 tps                = 120.f;
            ghuva::f32 time_multiplier    = 1.f;
            ghuva::f32 max_tps            = 100'000.f;
            ghuva::u64 ticks              = 0; // Since last loop.
            bool       parallel_ticking   = true; // Is parallel ticking enabled on the engine ?

            // Main loop config stuff.
            bool has_own_thread = true; // Is the engine running on a dedicated thread ?
        } engine;

        // You cleanup after yourself, we only want a view into these vectors.
        // You are expected to verify that all objects have valid mesh_ids and
        // only the mesh_ids used are in the mesh vector.
        // NOTE: notice how these are not const*, we WILL modify their contents.
        ghuva::mesh* meshes       = nullptr;
        ghuva::u64   mesh_count   = 0;
        object *     objects      = nullptr;
        ghuva::u64   object_count = 0;

        struct /* camera */
        {
            ghuva::transform t = {};

            // NOTE: These may be modified by the UI in between calls.
            // Perspective transform stuff.
            ghuva::f32 focal_len  = 0.75f;
            ghuva::f32 near_plane = 0.01f;
            ghuva::f32 far_plane  = 100.f;
        } camera;
    } params;

    struct /* outputs */ // Read these and communicate them back to the engine.
    {
        bool do_engine_config_update = true; // Whether or not to communicate these
                                             // params back to the engine.
        ghuva::f32 target_tps             = 120.0f;
        ghuva::f32 target_time_multiplier = 1.0f;
        bool parallel_ticking             = true;

        bool engine_has_dedicated_thread  = true;
    } outputs;

    app();
    ~app();

    // Throws ghuva::context_error on error.
    auto init() -> void;

    using loop_callback = ghuva::context::loop_message(*)(app&, ghuva::f32 dt, void*);
    auto loop(void* userdata, loop_callback) -> void;

private:
    auto loop_impl(ghuva::f32 dt) -> ghuva::context::loop_message;
    auto sync_params_to_outputs() -> void;
    auto write_scene_uniform() -> void;
    auto build_scene_geometry() -> void;
    auto write_geometry_buffers() -> void;
    auto do_ui(ghuva::f32 dt) -> void;
        auto ui_help(const char*) -> void;
        auto ui_draw_projection_window() -> void;
        auto ui_draw_limits_window() -> void;
        auto ui_draw_matrix(ghuva::m4f const& m, const char* panelname, const char* tablename) -> void;
    auto compute_transform_matrix_via_compute_pass() -> void;
    auto render() -> ghuva::context::loop_message;
        auto render_emit_draw_calls(wgpu::RenderPassEncoder render_pass) -> void;

    struct /* ui */
    {
        struct /* window */
        {
            bool projection   = false;
            bool adapter_info = false;
            bool imgui_demo   = false;
        } window;

        ghuva::u32 w = 1280;
        ghuva::u32 h = 400; // So it doesn't occlude the terminal on open.

        bool fixed_ticks_per_frame = false;
        ghuva::f32 cached_target_tps = 120.0f; // Cache the last tps when we toggle
                                               // fixed_ticks_per_frame so we can restore
                                               // it on the next toggle.

        ghuva::context::scene_uniforms scene_uniforms; // Cached for displaying to the user.
    } ui;

    // true  = use compute pass to calculate object transforms.
    // false = calculate object transforms on the cpu.
    bool compute_pass = true;

    ghuva::context& ctx;

    struct /* scene */
    {
        struct mesh_data
        {
            ghuva::u64 id;
            ghuva::u64 instance_count;
            ghuva::u64 start_index;
            ghuva::u64 index_count;
            ghuva::u64 start_vertex;
        };
        std::vector<mesh_data> geometry_offsets; // Where each mesh sits within the buffer.

        // Contains position + color + normal buffers. Divide size by 3 to get the offsets
        // for each buffer within this.
        ghuva::container<ghuva::context::vertex_t> geometry_buffer;
        ghuva::container<ghuva::context::index_t>  index_buffer;
        ghuva::container<ghuva::context::object_uniforms> instance_buffer;
    } scene;
};
