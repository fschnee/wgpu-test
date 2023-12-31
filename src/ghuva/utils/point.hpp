#pragma once

#include "aliases.hpp"

namespace ghuva
{
    template <typename NumT>
    struct point
    {
        NumT x;
        NumT y;
        NumT z;

        constexpr auto data() -> NumT* { return &x; }

        // Goes without saying, but caller must check for out of bounds access.
        constexpr auto operator[](const auto i)       -> NumT& { return *(data() + i); }
        constexpr auto operator[](const auto i) const -> NumT  { return *(data() + i); }

        [[nodiscard]] auto operator+(const point& other) const -> point;
        [[nodiscard]] auto operator-(const point& other) const -> point;
        [[nodiscard]] auto operator*(const NumT& num) const -> point;
        [[nodiscard]] auto operator/(const NumT& num) const -> point;

        [[nodiscard]] auto distance(const point& to) const -> NumT;
        [[nodiscard]] auto length() const -> NumT;
        [[nodiscard]] auto midpoint(const point& p) const -> point;

        [[nodiscard]] auto rotated(float radians, const point& axis = {0, 0}) const -> point;
        auto rotate(float radians, const point& axis = {0, 0}) -> point&;

        // NOTE: Returns {Nan, Nan} if length() == 0 due to division by 0,
        // use normalized_or({0, 0, 0}) if that's a problem.
        [[nodiscard]] auto normalized() const -> point;
        auto normalize() -> point&;

        [[nodiscard]] auto normalized_or(const point& p) const -> point;
        auto normalize_or(const point& p) -> point&;
    };

    template <typename NumT>
    [[nodiscard]] auto operator*(NumT num, const point<NumT>& p) -> point<NumT>;
    template <typename NumT>
    [[nodiscard]] auto operator/(NumT num, const point<NumT>& p) -> point<NumT>;

    using rpoint = point<double>;
    using fpoint = point<float>;
    using ipoint = point<i32>;
    using upoint = point<u32>;
}

// Extern instantiations (for faster #include time), instantiated in geometry.cpp.
//
// This + implementations being in the .cpp file means if you want these functions
// for other types (which realistically you won't don't) you will need to implement
// them yourself.
//
// This is a tradeoff between #include time and extendability.
//
// You can use LTO to regain (probably) regain the runtime-speed lost from not using
// constexpr but then linking takes forever.

extern template struct ghuva::point<double>;
extern template struct ghuva::point<float>;
extern template struct ghuva::point<ghuva::i32>;
extern template struct ghuva::point<ghuva::u32>;

extern template auto ghuva::operator*<double> (double, const point<double>&)   -> point<double>;
extern template auto ghuva::operator/<double> (double, const point<double>&)   -> point<double>;
extern template auto ghuva::operator*<float>  (float, const point<float>&)     -> point<float>;
extern template auto ghuva::operator/<float>  (float, const point<float>&)     -> point<float>;
extern template auto ghuva::operator*<ghuva::i32>(ghuva::i32, const point<ghuva::i32>&) -> point<ghuva::i32>;
extern template auto ghuva::operator/<ghuva::i32>(ghuva::i32, const point<ghuva::i32>&) -> point<ghuva::i32>;
extern template auto ghuva::operator*<ghuva::u32>(ghuva::u32, const point<ghuva::u32>&) -> point<ghuva::u32>;
extern template auto ghuva::operator/<ghuva::u32>(ghuva::u32, const point<ghuva::u32>&) -> point<ghuva::u32>;