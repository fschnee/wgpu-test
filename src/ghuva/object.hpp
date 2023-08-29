#pragma once

#include <functional>
#include <string>

#include "utils/point.hpp"
#include "utils/m4.hpp"
#include "transform.hpp"
#include "context.hpp"

namespace ghuva
{
    template< typename Engine >
    struct object
    {
        using engine_t   = Engine;
        using snapshot_t = typename engine_t::snapshot;

        using on_tick_t = std::function<void(
            object& self,
            float dt,
            snapshot_t const& snapshot,
            engine_t& engine
        )>;

        u64 id = 0; // Assigned by the engine. 0 = invalid.

        std::string name = "";
        u64 mesh_id = 0; // Registered by the engine. 0 = invalid.
        bool draw = true;
        bool tick = true;
        transform t = {};

        struct constructorargs
        {
            std::string name = "";
            transform t = {};
            on_tick_t on_tick = [](object&, float, snapshot_t const&, engine_t&){};
            u64 mesh_id = 0;
            bool draw = true;
            bool tick = true;
        };

        object(constructorargs&&);

        auto look_at(fpoint const& target) -> object&;

        on_tick_t on_tick = [](object&, float, snapshot_t const&, engine_t&){};
    };
}

// Impls.

template <typename E>
ghuva::object<E>::object(constructorargs&& args)
    : name{ ghuva::move(args.name) }
    , mesh_id{ args.mesh_id }
    , draw{ args.draw }
    , tick{ args.tick }
    , t{ ghuva::move(args.t) }
    , on_tick{ ghuva::move(args.on_tick) }
{}

template <typename E>
auto ghuva::object<E>::look_at(fpoint const& _target) -> object&
{
    //[[maybe_unused]] const auto target = _target - this->mt().pos;
    //TODO: Implementar. Só fazer this->mt().rot apontar target.

    return *this;
}

