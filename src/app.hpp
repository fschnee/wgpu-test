// Should be complately decoupled from engine, pass relevant info through main().
#pragma once

#include <vector>

#include "ghuva/context.hpp"

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
    };

    struct /* params */ // Set these from your own callback during loop.
    {
        struct /* engine */
        {
            ghuva::f32 total_time         = 0.f; // Can be different due to time dilation.
            ghuva::f32 tps                = 120.f;
            ghuva::f32 time_multiplier    = 1.f;
            ghuva::f32 max_tps            = 100'000.f;
            ghuva::u64 ticks              = 0; // Since last loop.
        } engine;

        // You cleanup after yourself, we only want a view into these vectors.
        ghuva::mesh const* meshes       = nullptr;
        ghuva::u64         mesh_count   = 0;
        object const*      objects      = nullptr;
        ghuva::u64         object_count = 0;

        // true  = use compute pass to calculate transforms.
        // false = calculate transforms on the cpu.
        bool transform_matrix_calculation_via_compute_pass = false;

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
        bool do_engine_config_update = false; // Whether or not to communicate these
                                              // params back to the engine.
        ghuva::f32 target_tps;
        ghuva::f32 target_time_multiplier;
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
        auto ui_draw_limits_window() -> void;
        auto ui_draw_matrix(ghuva::m4f const& m, const char* panelname, const char* tablename) -> void;
    auto compute_object_uniforms_directly() -> void;
    auto render() -> ghuva::context::loop_message;
        auto render_emit_draw_calls(wgpu::RenderPassEncoder render_pass) -> void;

    struct /* ui */
    {
        bool adapter_info_menu_open = false;

        ghuva::u32 w = 1280;
        ghuva::u32 h = 640;

        bool fixed_ticks_per_frame = false;
    } ui;

    ghuva::context& ctx;

    struct /* scene */
    {
        struct mesh_data
        {
            ghuva::u64 id;
            ghuva::u64 start_index;
            ghuva::u64 index_count;
            ghuva::u64 start_vertex;
        };
        std::vector<mesh_data> geometry_offsets; // Where each mesh sits within the buffer.

        ghuva::u8* buffer      = nullptr; // Contains all the geometry buffers.
        ghuva::u64 buffer_size = 0;

        // Point places within data.
        ghuva::u8* position_start = nullptr;
        ghuva::u8* color_start    = nullptr;
        ghuva::u8* normal_start   = nullptr;
        ghuva::u8* index_start    = nullptr;

        // The sizes of the individual buffers (position, color, normal, index), in bytes.
        ghuva::u64 vertex_buf_size = 0;
        ghuva::u64 index_buf_size  = 0;
    } scene;
};
