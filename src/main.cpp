#include <fmt/core.h>

#include <vector>
#include <thread>
#include <atomic>

// Unused for now.
//#include <rapidobj/rapidobj.hpp>

#include "ghuva/objects/eel.hpp"
#include "ghuva/objects/ew.hpp"
#include "ghuva/utils/cvt.hpp"
#include "ghuva/engine.hpp"

#include "app.hpp"

using namespace ghuva::aliases;
namespace g   = ghuva;

struct userdata
{
    using engine_t = g::default_engine;

    engine_t engine       = engine_t{};
    std::atomic_bool exit = false; // Flag to exit the engine_thread.
    std::jthread engine_thread; // The thread actually doing the ticking.

    u64 last_snapshot_tick = 0; // To keep track of how many ticks elapsed.

    // Since we need to have these survive more than 1 frame.
    std::vector<app::object> rendered_objs;
    std::vector<ghuva::mesh> meshes;

    auto engine_load_scene() -> void;
    auto engine_start_ticking() -> void;
};

int main()
{
    auto app = ::app{};

    // TODO: print something useful instead of just exiting.
    try                                        { app.init(); }
    catch (g::context::context_error const& e) { return 1; }

    auto ud = ::userdata{};
    ud.engine_load_scene();
    ud.engine_start_ticking();

    app.loop(&ud, [](::app& app, [[maybe_unused]] f32 dt, auto* _ud)
    {
        auto& ud      = *(_ud * g::cvt::rc<userdata*>);
        auto snapshot = ud.engine.take_snapshot(); // Take a copy of the latest snapshot.

        if(app.outputs.do_engine_config_update)
        {
            fmt::print(
                "[main] Requesting Engine to update time_multiplier to {} and tps to {}\n",
                app.outputs.target_time_multiplier,
                app.outputs.target_tps
            );

            using e = userdata::engine_t;

            ud.engine.post(e::set_time_multiplier{ .time_multiplier = app.outputs.target_time_multiplier });
            ud.engine.post(e::set_tps{ .tps = app.outputs.target_tps });
        }

        // Early return if nothing changed.
        if(snapshot.id == ud.last_snapshot_tick) return g::context::loop_message::do_continue;

        // Otherwise we set the params according to what we've got.
        app.params.engine = {
            .total_time      = snapshot.engine_config.total_time,
            .tps             = snapshot.engine_config.ticks_per_second,
            .time_multiplier = snapshot.engine_config.time_multiplier,
            .max_tps         = 100'000.f,
            .ticks           = snapshot.id - ud.last_snapshot_tick,
        };
        app.params.transform_matrix_calculation_via_compute_pass = false;

        ud.meshes             = ghuva::move(snapshot.meshes); // It's fine to steal, the snapshot is a copy.
        app.params.meshes     = ud.meshes.data();
        app.params.mesh_count = ud.meshes.size();

        ud.rendered_objs.clear();
        ud.rendered_objs.reserve(snapshot.objects.size()); // TODO: Guesstimation.
        for(auto const& obj : snapshot.objects)
        {
            if(obj.draw && obj.mesh_id != 0)
                ud.rendered_objs.push_back({ .mesh_id = obj.mesh_id, .t = obj.t });

            // Update camera transform if this is the camera object.
            if(obj.id == snapshot.camera_object_id)
                app.params.camera.t = obj.t;
        }
        app.params.objects      = ud.rendered_objs.data();
        app.params.object_count = ud.rendered_objs.size();

        ud.last_snapshot_tick = snapshot.id;

        // And surrender control to app for it to render stuff.
        return g::context::loop_message::do_continue;
    });

    ud.exit = true; // So the engine thread also stops.

    return 0;
}

auto userdata::engine_load_scene() -> void
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
        auto const& e = _e * g::cvt::rc<engine_t::e_register_mesh const&>;
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

        #if 0
        fmt::print("requesting a bunch-a pyramids, hold on\n");
        for(auto i = 0_u64; i < 80'000; ++i)
            engine.post(engine_t::register_object{ .object = {{
                .name = "Pyramid 2",
                .t = {
                    .pos = {0.5f, 0.0f, 0.0f},
                    .scale = {0.3f, 0.3f, 0.3f},
                },
                .on_tick = [](auto& self, auto dt, auto const&, auto&) { self.t.rot.z += dt; },
                .mesh_id = mesh_id,
            }}});
        #endif
    })});
    fmt::print("[main.load_scene] Requested engine to register EW for {{ .event_id = {} }}\n", ew_post_id);
}

auto userdata::engine_start_ticking() -> void
{
    engine_thread = std::jthread{
        [this, stopwatch = g::chrono::stopwatch()]() mutable
        {
            while(!exit) engine.tick(stopwatch.click().last_segment());
        }
    };
}
