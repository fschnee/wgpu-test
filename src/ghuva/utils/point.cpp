#include "point.hpp"
#include "cvt.hpp"

#include <numeric> // std::midpoint.
#include <cmath> // std::sin, std::cos, std::sqrt.

// Implementations.

template <typename NumT>
auto ghuva::point<NumT>::operator+(const point& other) const -> point
{ return {x + other.x, y + other.y, z + other.z}; }

template <typename NumT>
auto ghuva::point<NumT>::operator-(const point& other) const -> point
{ return {x - other.x, y - other.y, z - other.z}; }

template <typename NumT>
auto ghuva::point<NumT>::operator*(const NumT& num) const -> point
{ return {x * num, y * num, z * num}; }

template <typename NumT>
auto ghuva::point<NumT>::operator/(const NumT& num) const -> point
{ return {x / num, y / num, z / num}; }

template <typename NumT>
auto ghuva::operator*(NumT num, const point<NumT>& p) -> point<NumT>
{ return p * num; }

template <typename NumT>
auto ghuva::operator/(NumT num, const point<NumT>& p) -> point<NumT>
{ return p / num; }

template <typename NumT>
auto ghuva::point<NumT>::distance(const point& to) const -> NumT
{
    const auto diff = *this - to;
    return cvt::toe * std::sqrt(cvt::to<double>(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z));
}

template <typename NumT>
auto ghuva::point<NumT>::length() const -> NumT
{ return distance({0, 0, 0}); }

template <typename NumT>
auto ghuva::point<NumT>::midpoint(const point& p) const -> point
{ return {std::midpoint(x, p.x), std::midpoint(y, p.y), std::midpoint(z, p.z)}; }

// TODO: implement me for 3d.
template <typename NumT>
auto ghuva::point<NumT>::rotated(float radians, const point& axis) const -> point
{
    return
    {
        .x = cvt::toe( std::cos(cvt::to<double> * radians) * cvt::to<double>(x - axis.x) - std::sin(cvt::to<double> * radians) * cvt::to<double>(y - axis.y) + cvt::to<double> * axis.x ),
        .y = cvt::toe( std::sin(cvt::to<double> * radians) * cvt::to<double>(x - axis.x) + std::cos(cvt::to<double> * radians) * cvt::to<double>(y - axis.y) + cvt::to<double> * axis.y ),
        .z = 0
    };
}

template <typename NumT>
auto ghuva::point<NumT>::rotate(float radians, const point& axis) -> point&
{ return (*this = rotated(radians, axis)); }

template <typename NumT>
auto ghuva::point<NumT>::normalized() const -> point
{ return *this / length(); }

template <typename NumT>
auto ghuva::point<NumT>::normalize() -> point&
{ return (*this = normalized()); }

template <typename NumT>
auto ghuva::point<NumT>::normalized_or(const point& p) const -> point
{
    auto len = length();
    if(len >= 0) { return *this / len; }
    else         { return p; }
}

template <typename NumT>
auto ghuva::point<NumT>::normalize_or(const point& p) -> point&
{ return (*this = normalized_or(p)); }

// Explicit instantiations.

template struct ghuva::point<double>;
template struct ghuva::point<float>;
template struct ghuva::point<ghuva::i32>;
template struct ghuva::point<ghuva::u32>;

template auto ghuva::operator*<double> (double, const point<double>&)   -> point<double>;
template auto ghuva::operator/<double> (double, const point<double>&)   -> point<double>;
template auto ghuva::operator*<float>  (float, const point<float>&)     -> point<float>;
template auto ghuva::operator/<float>  (float, const point<float>&)     -> point<float>;
template auto ghuva::operator*<ghuva::i32>(ghuva::i32, const point<ghuva::i32>&) -> point<ghuva::i32>;
template auto ghuva::operator/<ghuva::i32>(ghuva::i32, const point<ghuva::i32>&) -> point<ghuva::i32>;
template auto ghuva::operator*<ghuva::u32>(ghuva::u32, const point<ghuva::u32>&) -> point<ghuva::u32>;
template auto ghuva::operator/<ghuva::u32>(ghuva::u32, const point<ghuva::u32>&) -> point<ghuva::u32>;
