#pragma once

#include <algorithm>
#include <cmath>

namespace ghuva::math
{
    template <typename ReturnT = int>
    constexpr auto sign(auto v) { return v >= decltype(v){0} ? ReturnT{1} : ReturnT{-1}; }

    // Saturating lerp.
    constexpr auto satlerp(auto now, auto target, auto factor)
    {
        using ft = decltype(factor);
        factor = std::clamp(factor, ft{0}, ft{1});
        return now * (1 - factor) + target * factor;
    }

    // Arduino-style function for mapping values of different ranges.
    // e.x: 10 == map(25, -25, 25, 0, 10);
    constexpr auto map(auto val, auto r1min, auto r1max, auto r2min, auto r2max)
    {
        return (val - r1min) * (r2max - r2min) / (r1max - r1min) + r2min;
    }
    constexpr auto map(auto val, auto r1, auto r2) { return map(val, r1[0], r1[1], r2[0], r2[1]); }
}

namespace ghuva{ namespace m = ghuva::math; }
