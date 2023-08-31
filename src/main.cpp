#include <fmt/core.h>

#include <atomic>
#include <random>
#include <thread>
#include <vector>

// Unused for now.
//#include <rapidobj/rapidobj.hpp>

#include "ghuva/meshes/pyramid.hpp"
#include "ghuva/objects/eel.hpp"
#include "ghuva/objects/ew.hpp"
#include "ghuva/utils/math.hpp"
#include "ghuva/utils/cvt.hpp"
#include "ghuva/engine.hpp"

#include "app.hpp"

using namespace ghuva::aliases;
namespace g   = ghuva;

struct userdata
{
    using engine_t = g::default_engine;

    engine_t engine          = engine_t{};
    std::atomic_bool exit    = true;  // Flag to exit the engine_thread.
    std::atomic_bool ticking = false; // Flag to tell whether or not engine_thread is alive.
    std::jthread engine_thread; // The thread actually doing the ticking.
    g::chrono::default_stopwatch engine_stopwatch = g::chrono::stopwatch{};

    u64 last_snapshot_tick = 0; // To keep track of how many ticks elapsed.

    // Since we need to have these survive more than 1 frame.
    std::vector<app::object> rendered_objs;
    std::vector<g::mesh>     meshes;

    auto engine_tick(bool dedicated_thread) -> void;
    auto engine_load_scene() -> void;

private:
    auto engine_thread_start_ticking() -> void;
};

int main()
{
    auto app = ::app{};

    // TODO: print something useful instead of just exiting.
    try                                        { app.init(); }
    catch (g::context::context_error const& e) { return 1; }

    auto ud = ::userdata{};
    ud.engine_load_scene();

    app.loop(&ud, [](::app& app, [[maybe_unused]] f32 dt, auto* _ud)
    {
        auto& ud      = *(_ud * g::cvt::rc<userdata*>);
        // TODO: query only the snapshot id, if new_id > ud.last_snapshot_id we actually take it.

        // We communicate engine config updates first otherwise they may be lost between ticks.
        if(app.outputs.do_engine_config_update)
        {
            fmt::print(
                "[main] Requesting Engine to update time_multiplier to {}, tps to {} and parallel_ticking to {}\n",
                app.outputs.target_time_multiplier,
                app.outputs.target_tps,
                app.outputs.parallel_ticking
            );

            using e = userdata::engine_t;

            ud.engine.post(e::set_time_multiplier{ .time_multiplier = app.outputs.target_time_multiplier });
            ud.engine.post(e::set_tps{ .tps = app.outputs.target_tps });
            ud.engine.post(e::set_parallel_ticking{ .parallel_ticking = app.outputs.parallel_ticking });
        }

        ud.engine_tick(app.outputs.engine_has_dedicated_thread);
        auto snapshot = ud.engine.take_snapshot(); // Take a copy of the latest snapshot.

        // Early return if nothing changed.
        if(snapshot.id == ud.last_snapshot_tick) return g::context::loop_message::do_continue;

        // Otherwise we set the params according to what we've got.

        app.params.engine = {
            .total_time       = snapshot.engine_config.total_time,
            .tps              = snapshot.engine_config.ticks_per_second,
            .time_multiplier  = snapshot.engine_config.time_multiplier,
            .max_tps          = 100'000.f,
            .ticks            = snapshot.id - ud.last_snapshot_tick,
            .parallel_ticking = snapshot.engine_config.parallel_ticking,

            .has_own_thread = ud.ticking,
        };
        ud.meshes             = ghuva::move(snapshot.meshes); // It's fine to steal, the snapshot is a copy.
        app.params.meshes     = ud.meshes.data();
        app.params.mesh_count = ud.meshes.size();

        ud.rendered_objs.clear();
        ud.rendered_objs.reserve(snapshot.objects.size()); // TODO: Guesstimation.
        for(auto const& obj : snapshot.objects)
        {
            if(obj.draw && obj.mesh_id != 0)
                ud.rendered_objs.push_back({ .mesh_id = obj.mesh_id, .t = obj.t, .mesh_index = 0 /*dummy*/});

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
        .on_tick =
            [
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
            ](auto& self, auto dt, auto const& snapshot, auto& engine) mutable
            {
                if(snapshot.camera_object_id != self.id)
                    engine.post(engine_t::set_camera{ .object_id = self.id });

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
                self.t.pos = g::m::satlerp( self.t.pos, (1-t)*(1-t)*p1 + 2*(1-t)*t*p2 + t*t*p3, dt);
            },
    }}});
    fmt::print("[main.load_scene] Requested engine to register Camera {{ .event_id = {} }}\n", camera_post_id);

    auto const pyramid_mesh_post_id = engine.post(engine_t::register_mesh{ ghuva::meshes::pyramid });
    fmt::print("[main.load_scene] Requested engine to register pyramid_mesh {{ .event_id = {} }}\n", pyramid_mesh_post_id);

    // Other pyramids, just to have more data on the engine.
    for(auto i = 0_u64; i < 10; ++i)
        engine.post(engine_t::register_mesh{ ghuva::meshes::pyramid });

    // Our pyramid loader will wait for the (original) mesh to be registered to get it's id.
    auto const ew_post_id = engine.post(engine_t::register_object{ g::objects::make_ew<engine_t>(pyramid_mesh_post_id, [](
        auto const& _e, auto, auto const&, auto& engine
    ){
        // Due to EW checking all event types we need to tell the compiler exactly what kind of event this is.
        // In this situation we know the event type (posted it just above), but in the middle of game code
        // it could get get messy and easy to make mistakes. Don't blindly cast like I do here.
        // TODO: EW for single event type -> add template param.
        auto const& e = _e * g::cvt::rc<engine_t::e_register_mesh const&>;
        auto const mesh_id = e.body.mesh.id;

        engine.post(engine_t::register_object{ .object = {{
            .name = "Pyramid loader",
            .on_tick = [
                mesh_id,
                gen   = std::default_random_engine{},
                xposdist  = std::uniform_real_distribution<f32>(-8.0, 8.0),
                yposdist  = std::uniform_real_distribution<f32>(-8.0, 8.0),
                zposdist  = std::uniform_real_distribution<f32>(1.0, 9.0),
                rotdist   = std::uniform_real_distribution<f32>(-3.1415, 3.1415),
                remaining = 16'000_u64
            ](auto&, auto, auto const& snapshot, auto& engine) mutable
            {
                if(remaining <= 0) return;

                engine.post(engine_t::register_object{ .object{{
                    .name = "Pyramid",
                    .t = {
                        .pos   = {xposdist(gen),  yposdist(gen),  zposdist(gen)},
                        .rot   = {rotdist(gen),   rotdist(gen),   rotdist(gen)},
                        .scale = {0.1f,           0.1f,           0.1f},
                    },
                    .on_tick = [remaining](auto& self, auto dt, auto const&, auto&) { self.t.rot[remaining % 3] += dt; },
                    .mesh_id = mesh_id,
                }}});
                fmt::print("[ghuva::engine/t{}][main.load_scene.pyramid_loader] Requested engine to register Pyramid\n", snapshot.id);

                --remaining;
            }
        }}});
    })});
    fmt::print("[main.load_scene] Requested engine to register EW for {{ .event_id = {} }}\n", ew_post_id);
}

auto userdata::engine_tick(bool dedicated_thread) -> void
{
    bool const requested_exit = exit;
    bool const exited = !ticking;

    auto const is_alive = !requested_exit && !exited;
    auto const crashed  = !requested_exit && exited; // *Should* be impossible.
    auto const is_dying = requested_exit && !exited;
    auto const is_dead  = requested_exit && exited;

    auto const start_thread    = dedicated_thread && is_dead;
    auto const continue_thread = dedicated_thread && is_alive;
    auto const tick_myself  = !dedicated_thread && is_dead;
    auto const request_exit = !dedicated_thread && is_alive;
    auto const wait_exit    = !dedicated_thread && is_dying;

         if(start_thread)    { fmt::print("[main] Requested engine_thread to start ticking.\n"); engine_thread_start_ticking(); }
    else if(crashed)         { fmt::print("[main] engine_thread crashed, asked to restart\n");   engine_thread_start_ticking(); }
    else if(continue_thread) { /* fmt::print("[main] engine_thread is ticking\n"); */ }
    else if(tick_myself)     { engine.tick(engine_stopwatch.click().last_segment()); }
    else if(request_exit)    { exit = true; fmt::print("[main] Requested engine_thread to stop ticking.\n"); }
    else if(wait_exit)       { fmt::print("[main] engine_thread is dying, waiting to take over ticking.\n"); }
}

auto userdata::engine_thread_start_ticking() -> void
{
    exit = false;
    ticking = true;
    engine_thread = std::jthread{ [this]() {
        fmt::print("[engine_thread] Starting\n");
        // TODO: sleep between ticks.
        while(!this->exit) engine.tick(this->engine_stopwatch.click().last_segment());
        fmt::print("[engine_thread] Exiting\n");
        this->ticking = false;
    }};
}
