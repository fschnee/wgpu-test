#pragma once

#include <functional>
#include <string>
#include <vector>

#include "utils/point.hpp"
#include "utils/m4.hpp"
#include "context.hpp"

namespace ghuva
{
    struct object
    {
        using vecf = std::vector<context::vertex_t>;
        using vecidx = std::vector<context::index_t>;
        using on_tick_t = std::function<void(float tick_dt, object& self)>;

        std::string name = "";
        bool draw = true;
        bool tick = true;

        struct mesh_t
        {
            vecf   vertexes = {};
            vecf   colors   = {};
            vecf   normals  = {};
            vecidx indexes  = {};
        } mesh;

        struct transform
        {
            fpoint pos   = {0, 0, 0};
            fpoint rot   = {0, 0, 0}; // In radians.
            fpoint scale = {1, 1, 1};
        };

        struct constructorargs
        {
            std::string name = "";
            mesh_t mesh = {};
            transform world_transform = {};
            transform model_transform = {};
            on_tick_t on_tick = [](auto, auto){};
            bool draw = true;
            bool tick = true;
        };

        object(constructorargs&&);

        // wt = world_transform;
        // mt = model_transform;
        constexpr auto wt() -> transform&;
        constexpr auto mt() -> transform&;
        constexpr auto wt() const -> transform const&;
        constexpr auto mt() const -> transform const&;

        auto look_at(fpoint const& target) -> object&;

        // This is heavy, please only call this if you *really* need to,
        // otherwise this is called for every object before rendering.
        constexpr auto compute_transform() const -> ghuva::m4f;

        on_tick_t on_tick = [](auto, auto){};

    private:
        transform world_transform = {};
        transform model_transform = {};

        // For optimization purposes.
        bool mutable transform_is_dirty = true;
        m4f  mutable cached_transform   = ghuva::m4f::scaling(1, 1, 1);
    };

}

// Impls.

constexpr auto ghuva::object::wt() -> transform& { transform_is_dirty = true; return world_transform; }
constexpr auto ghuva::object::mt() -> transform& { transform_is_dirty = true; return model_transform; }
constexpr auto ghuva::object::wt() const -> transform const& { return world_transform; }
constexpr auto ghuva::object::mt() const -> transform const& { return model_transform; }

constexpr auto ghuva::object::compute_transform() const -> m4f
{
    if(!this->transform_is_dirty) return this->cached_transform;
    this->transform_is_dirty = false;

    const auto& wt = this->wt();
    const auto& mt = this->mt();

    return this->cached_transform = ghuva::m4f
        ::translation(wt.pos[0], wt.pos[1], wt.pos[2])
        .zRotate(wt.rot[2])
        .yRotate(wt.rot[1])
        .xRotate(wt.rot[0])
        .scale(wt.scale[0], wt.scale[1], wt.scale[2])
        .translate(mt.pos[0], mt.pos[1], mt.pos[2])
        .zRotate(mt.rot[2])
        .yRotate(mt.rot[1])
        .xRotate(mt.rot[0])
        .scale(mt.scale[0], mt.scale[1], mt.scale[2]);
};