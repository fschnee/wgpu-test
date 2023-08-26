// The event waiter object.
// Waits indefinetively.
// TODO: make a waiter for a X ticks ?
#pragma once

#include <fmt/core.h>

#include "../object.hpp"
#include "../engine.hpp"
#include "../utils/forward.hpp"

namespace ghuva::objects
{
    template <typename Engine, typename F>
    constexpr auto make_ew(u64 id, F&& f) -> object<Engine>
    {
        using delete_object = typename Engine::delete_object;

        return {{
            .name = fmt::format("EW-e{}", id),
            .on_tick = [id = id, f = ghuva::forward<F>(f)](auto& self, [[maybe_unused]] auto dt, auto const& snapshot, auto& engine){
                auto done = false;
                snapshot.all_posts([&](auto& el_vec){
                    for(auto const& e : el_vec)
                    {
                        if(done) return;
                        else if(e.id != id) continue;

                        f(e, dt, snapshot, engine);
                        engine.post(delete_object{ .id = self.id, .success = false /* dummy */ }, self.id);
                        done = true;
                    }
                });
            },
            .draw = false,
        }};
    }
}