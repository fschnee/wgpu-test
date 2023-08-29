#pragma once

#include "utils/point.hpp"

namespace ghuva
{
    struct transform
    {
        fpoint pos   = {0, 0, 0};
        fpoint rot   = {0, 0, 0}; // In radians.
        fpoint scale = {1, 1, 1};
    };
}