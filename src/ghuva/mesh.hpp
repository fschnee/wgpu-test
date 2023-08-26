#pragma once

#include <vector>

#include "context.hpp"

namespace ghuva
{
    struct mesh
    {
        using vecf = std::vector<context::vertex_t>;
        using vecidx = std::vector<context::index_t>;

        u64 id; // Assigned by the engine.

        vecf   vertexes = {};
        vecf   colors   = {};
        vecf   normals  = {};
        vecidx indexes  = {};
    };
};
