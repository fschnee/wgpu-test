#include "geometry.hpp"
#include "cvt.hpp"

#include <cmath> // std::sin, std::cos, std::sqrt.
#include <numeric> // std::midpoint.

// Implementations.

template <typename NumT>
auto standalone::point<NumT>::operator+(const point& other) const -> point
{ return {x + other.x, y + other.y, z + other.z}; }

template <typename NumT>
auto standalone::point<NumT>::operator-(const point& other) const -> point
{ return {x - other.x, y - other.y, z - other.z}; }

template <typename NumT>
auto standalone::point<NumT>::operator*(const NumT& num) const -> point
{ return {x * num, y * num, z * num}; }

template <typename NumT>
auto standalone::point<NumT>::operator/(const NumT& num) const -> point
{ return {x / num, y / num, z / num}; }

template <typename NumT>
auto standalone::operator*(NumT num, const point<NumT>& p) -> point<NumT>
{ return p * num; }

template <typename NumT>
auto standalone::operator/(NumT num, const point<NumT>& p) -> point<NumT>
{ return p / num; }

template <typename NumT>
auto standalone::point<NumT>::distance(const point& to) const -> NumT
{
    const auto diff = *this - to;
    return cvt::toe * std::sqrt(cvt::to<double>(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z));
}

template <typename NumT>
auto standalone::point<NumT>::length() const -> NumT
{ return distance({0, 0, 0}); }

template <typename NumT>
auto standalone::point<NumT>::midpoint(const point& p) const -> point
{ return {std::midpoint(x, p.x), std::midpoint(y, p.y), std::midpoint(z, p.z)}; }

// TODO: implement me for 3d.
template <typename NumT>
auto standalone::point<NumT>::rotated(float radians, const point& axis) const -> point
{
    return
    {
        .x = cvt::toe( std::cos(cvt::to<double> * radians) * cvt::to<double>(x - axis.x) - std::sin(cvt::to<double> * radians) * cvt::to<double>(y - axis.y) + cvt::to<double> * axis.x ),
        .y = cvt::toe( std::sin(cvt::to<double> * radians) * cvt::to<double>(x - axis.x) + std::cos(cvt::to<double> * radians) * cvt::to<double>(y - axis.y) + cvt::to<double> * axis.y ),
        .z = 0
    };
}

template <typename NumT>
auto standalone::point<NumT>::rotate(float radians, const point& axis) -> point&
{ return (*this = rotated(radians, axis)); }

template <typename NumT>
auto standalone::point<NumT>::normalized() const -> point
{ return *this / length(); }

template <typename NumT>
auto standalone::point<NumT>::normalize() -> point&
{ return (*this = normalized()); }

template <typename NumT>
auto standalone::point<NumT>::normalized_or(const point& p) const -> point
{
    auto len = length();
    if(len >= 0) { return *this / len; }
    else         { return p; }
}

template <typename NumT>
auto standalone::point<NumT>::normalize_or(const point& p) -> point&
{ return (*this = normalized_or(p)); }

// Explicit instantiations.

template struct standalone::point<double>;
template struct standalone::point<float>;
template struct standalone::point<standalone::i32>;
template struct standalone::point<standalone::u32>;

template auto standalone::operator*<double> (double, const point<double>&)   -> point<double>;
template auto standalone::operator/<double> (double, const point<double>&)   -> point<double>;
template auto standalone::operator*<float>  (float, const point<float>&)     -> point<float>;
template auto standalone::operator/<float>  (float, const point<float>&)     -> point<float>;
template auto standalone::operator*<standalone::i32>(standalone::i32, const point<standalone::i32>&) -> point<standalone::i32>;
template auto standalone::operator/<standalone::i32>(standalone::i32, const point<standalone::i32>&) -> point<standalone::i32>;
template auto standalone::operator*<standalone::u32>(standalone::u32, const point<standalone::u32>&) -> point<standalone::u32>;
template auto standalone::operator/<standalone::u32>(standalone::u32, const point<standalone::u32>&) -> point<standalone::u32>;
