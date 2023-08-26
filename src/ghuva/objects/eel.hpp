// The Engine event logger object.
#pragma once

#include <type_traits>
#include <vector>

#include <fmt/core.h>

#include "../object.hpp"
#include "../engine.hpp"
#include "../utils/type_name.hpp"
#include "../utils/remove_cvref.hpp"

namespace ghuva::objects::eel_impl
{
    template <typename T, typename VecType>
    constexpr auto is_vec_of() -> bool
    {
        using el_t = ghuva::remove_cvref_t< VecType >::value_type;
        return std::is_same_v< ghuva::remove_cvref_t< T >, el_t >;
    }

    constexpr auto on_register_object(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Engine registered object {{ .id={}, .name=\"{}\" }}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id,
            e.body.object.id, e.body.object.name
        );
    }

    constexpr auto on_register_mesh(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Engine registered mesh {{ .id={}, .vertexes={}, .indexes={} }}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id,
            e.body.mesh.id, e.body.mesh.vertexes.size(), e.body.mesh.indexes.size()
        );
    }

    constexpr auto on_delete_object(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Engine {} object {{ .id={} }}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id,
            e.body.success ? "successfully deleted" : "FAILED deletion of", e.body.id
        );
    }

    constexpr auto on_change_tps(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Engine changed tps {{ .tps={} }}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id,
            e.body.tps
        );
    }

    constexpr auto on_set_camera(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Engine set camera {{ .object_id={} }}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id,
            e.body.object_id
        );
    }

    constexpr auto on_unkown_event(auto const& snapshot, auto& self, auto const& e)
    {
        fmt::print(
            "[ghuva::engine/t{}][o{}/{}][e{}/o{}] Unknown event found: {}\n",
            snapshot.id, self.id, self.name, e.id, e.source_object_id, ghuva::clean_type_name(e)
        );
    }

    constexpr auto on_tick(
        auto& self,
        [[maybe_unused]] auto dt,
        auto const& snapshot,
        [[maybe_unused]] auto& engine
    ) {
        using engine_t = ghuva::remove_cvref_t< decltype(self) >::engine_t;

        snapshot.all_posts([&](auto& el_vec){
            #define GHUVA_EEL_FOREACH(event_type, func)                       \
                if constexpr(is_vec_of<event_type, decltype(el_vec)>()) { \
                    for(auto const& e : el_vec) func(snapshot, self, e);  \
                }

                 GHUVA_EEL_FOREACH( typename engine_t::e_register_object, on_register_object )
            else GHUVA_EEL_FOREACH( typename engine_t::e_register_mesh,   on_register_mesh )
            else GHUVA_EEL_FOREACH( typename engine_t::e_delete_object,   on_delete_object )
            else GHUVA_EEL_FOREACH( typename engine_t::e_change_tps,      on_change_tps )
            else GHUVA_EEL_FOREACH( typename engine_t::e_set_camera,      on_set_camera )
            else { for(auto const& e : el_vec) on_unkown_event(snapshot, self, e); }
        });
    }
}

namespace ghuva::objects
{
    template <typename Engine>
    constexpr auto make_eel() -> object<Engine>
    {
        return {{
            .name = "EEL",
            .on_tick = [](auto se, auto dt, auto sn, auto en){
                eel_impl::on_tick(se, dt, sn, en);
            },
            .draw = false,
        }};
    }
}