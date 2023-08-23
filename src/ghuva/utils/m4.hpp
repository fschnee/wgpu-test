#pragma once

#include <cmath>
#include <numbers>

#include "forward.hpp"
#include "aliases.hpp"

namespace ghuva
{
    template <typename T>
    struct m4
    {
        constexpr m4() = default;

        // Move ops.
        constexpr m4(m4&&);
        constexpr auto operator=(m4&&) -> m4&;

        // Copy ops.
        constexpr m4(m4 const&);
        constexpr auto operator=(m4 const&) -> m4&;

        // Utils.
        constexpr auto do_dot(m4 const& other) -> m4&;
        constexpr auto dot(m4 const& other) const -> m4;
        static constexpr auto xRotation(auto angleInRadians) -> m4;
        static constexpr auto yRotation(auto angleInRadians) -> m4;
        static constexpr auto zRotation(auto angleInRadians) -> m4;
        static constexpr auto scaling(T const& sx, T const& sy, T const& sz) -> m4;
        static constexpr auto translation(T const& tx, T const& ty, T const& tz) -> m4;
        constexpr auto xRotate (auto angleInRadians) -> m4&                           { return this->do_dot(m4::xRotation(angleInRadians)); }
        constexpr auto xRotated(auto angleInRadians) const -> m4                      { return this->dot(   m4::xRotation(angleInRadians)); }
        constexpr auto yRotate (auto angleInRadians) -> m4&                           { return this->do_dot(m4::yRotation(angleInRadians)); }
        constexpr auto yRotated(auto angleInRadians) const -> m4                      { return this->dot(   m4::yRotation(angleInRadians)); }
        constexpr auto zRotate (auto angleInRadians) -> m4&                           { return this->do_dot(m4::zRotation(angleInRadians)); }
        constexpr auto zRotated(auto angleInRadians) const -> m4                      { return this->dot(   m4::zRotation(angleInRadians)); }
        constexpr auto scale (T const& sx, T const& sy, T const& sz) -> m4&           { return this->do_dot(m4::scaling(sx, sy, sz)); }
        constexpr auto scaled(T const& sx, T const& sy, T const& sz) const -> m4      { return this->dot(   m4::scaling(sx, sy, sz)); }
        constexpr auto translate (T const& tx, T const& ty, T const& tz) -> m4&       { return this->do_dot(m4::translation(tx, ty, tz)); }
        constexpr auto translated(T const& tx, T const& ty, T const& tz) const -> m4  { return this->dot(   m4::translation(tx, ty, tz)); }

        struct perspectiveparams{ T focal_len, aspect_ratio, near, far; };
        static constexpr auto perspective(perspectiveparams const&) -> m4;

        // Members.
        T raw[4][4] = {0};
    };

    using m4d = m4<double>;
    using m4f = m4<float>;

    // Impls.

    template <typename T>
    constexpr m4<T>::m4(m4&& other) { *this = ghuva::move(other); }

    template <typename T>
    constexpr auto m4<T>::operator=(m4&& other) -> m4&
    {
        for(auto i = 0; i < 4; ++i) for(auto j = 0; j < 4; ++j) this->raw[i][j] = ghuva::move(other.raw[i][j]);
        return *this;
    }

    template <typename T>
    constexpr m4<T>::m4(m4 const& other) { *this = other; }

    template <typename T>
    constexpr auto m4<T>::operator=(m4 const& other) -> m4&
    {
        for(auto i = 0; i < 4; ++i) for(auto j = 0; j < 4; ++j) this->raw[i][j] = other.raw[i][j];
        return *this;
    }

    template <typename T>
    constexpr auto m4<T>::do_dot(m4 const& b) -> m4&
    {
        auto b00 = b.raw[0][0];
        auto b01 = b.raw[0][1];
        auto b02 = b.raw[0][2];
        auto b03 = b.raw[0][3];
        auto b10 = b.raw[1][0];
        auto b11 = b.raw[1][1];
        auto b12 = b.raw[1][2];
        auto b13 = b.raw[1][3];
        auto b20 = b.raw[2][0];
        auto b21 = b.raw[2][1];
        auto b22 = b.raw[2][2];
        auto b23 = b.raw[2][3];
        auto b30 = b.raw[3][0];
        auto b31 = b.raw[3][1];
        auto b32 = b.raw[3][2];
        auto b33 = b.raw[3][3];
        auto a00 = this->raw[0][0];
        auto a01 = this->raw[0][1];
        auto a02 = this->raw[0][2];
        auto a03 = this->raw[0][3];
        auto a10 = this->raw[1][0];
        auto a11 = this->raw[1][1];
        auto a12 = this->raw[1][2];
        auto a13 = this->raw[1][3];
        auto a20 = this->raw[2][0];
        auto a21 = this->raw[2][1];
        auto a22 = this->raw[2][2];
        auto a23 = this->raw[2][3];
        auto a30 = this->raw[3][0];
        auto a31 = this->raw[3][1];
        auto a32 = this->raw[3][2];
        auto a33 = this->raw[3][3];

        this->raw[0][0] = b00 * a00 + b01 * a10 + b02 * a20 + b03 * a30;
        this->raw[0][1] = b00 * a01 + b01 * a11 + b02 * a21 + b03 * a31;
        this->raw[0][2] = b00 * a02 + b01 * a12 + b02 * a22 + b03 * a32;
        this->raw[0][3] = b00 * a03 + b01 * a13 + b02 * a23 + b03 * a33;
        this->raw[1][0] = b10 * a00 + b11 * a10 + b12 * a20 + b13 * a30;
        this->raw[1][1] = b10 * a01 + b11 * a11 + b12 * a21 + b13 * a31;
        this->raw[1][2] = b10 * a02 + b11 * a12 + b12 * a22 + b13 * a32;
        this->raw[1][3] = b10 * a03 + b11 * a13 + b12 * a23 + b13 * a33;
        this->raw[2][0] = b20 * a00 + b21 * a10 + b22 * a20 + b23 * a30;
        this->raw[2][1] = b20 * a01 + b21 * a11 + b22 * a21 + b23 * a31;
        this->raw[2][2] = b20 * a02 + b21 * a12 + b22 * a22 + b23 * a32;
        this->raw[2][3] = b20 * a03 + b21 * a13 + b22 * a23 + b23 * a33;
        this->raw[3][0] = b30 * a00 + b31 * a10 + b32 * a20 + b33 * a30;
        this->raw[3][1] = b30 * a01 + b31 * a11 + b32 * a21 + b33 * a31;
        this->raw[3][2] = b30 * a02 + b31 * a12 + b32 * a22 + b33 * a32;
        this->raw[3][3] = b30 * a03 + b31 * a13 + b32 * a23 + b33 * a33;

        return *this;
    }

    template <typename T>
    constexpr auto m4<T>::dot(m4 const& other) const -> m4
    {
        auto ret = *this;
        return ret.do_dot(other);
    }

    template <typename T>
    constexpr auto m4<T>::xRotation(auto angleInRadians) -> m4
    {
        auto c = std::cos(angleInRadians);
        auto s = std::sin(angleInRadians);

        // ret = {
        //   1, 0, 0, 0,
        //   0, c, s, 0,
        //   0, -s, c, 0,
        //   0, 0, 0, 1,
        // };

        auto ret = m4{};
        ret.raw[0][0] = 1;
        ret.raw[1][1] = c;
        ret.raw[1][2] = s;
        ret.raw[2][1] = -s;
        ret.raw[2][2] = c;
        ret.raw[3][3] = 1;
        return ret;
    }

    template <typename T>
    constexpr auto m4<T>::yRotation(auto angleInRadians) -> m4
    {
        auto c = std::cos(angleInRadians);
        auto s = std::sin(angleInRadians);

        // ret = {
        //     c, 0, -s, 0,
        //     0, 1, 0, 0,
        //     s, 0, c, 0,
        //     0, 0, 0, 1,
        // };

        auto ret = m4{};
        ret.raw[0][0] = c;
        ret.raw[0][2] = -s;
        ret.raw[1][1] = 1;
        ret.raw[2][0] = s;
        ret.raw[2][2] = c;
        ret.raw[3][3] = 1;
        return ret;
    }

    template <typename T>
    constexpr auto m4<T>::zRotation(auto angleInRadians) -> m4
    {
        auto c = std::cos(angleInRadians);
        auto s = std::sin(angleInRadians);

        // ret = {
        //    c, s, 0, 0,
        //   -s, c, 0, 0,
        //    0, 0, 1, 0,
        //    0, 0, 0, 1,
        // };

        auto ret = m4{};
        ret.raw[0][0] = c;
        ret.raw[0][1] = s;
        ret.raw[1][0] = -s;
        ret.raw[1][1] = c;
        ret.raw[2][2] = 1;
        ret.raw[3][3] = 1;
        return ret;
    }

    template <typename T>
    constexpr auto m4<T>::scaling(T const& sx, T const& sy, T const& sz) -> m4
    {
        // ret = {
        //    sx, 0,  0,  0,
        //    0, sy,  0,  0,
        //    0,  0, sz,  0,
        //    0,  0,  0,  1,
        // };

        auto ret = m4{};
        ret.raw[0][0] = sx;
        ret.raw[1][1] = sy;
        ret.raw[2][2] = sz;
        ret.raw[3][3] = 1;
        return ret;
    }

    template <typename T>
    constexpr auto m4<T>::translation(T const& tx, T const& ty, T const& tz) -> m4
    {
        // ret = {
        //    1,  0,  0,  0,
        //    0,  1,  0,  0,
        //    0,  0,  1,  0,
        //    tx, ty, tz, 1,
        // };

        auto ret = m4::scaling(1, 1, 1);
        ret.raw[3][0] = tx;
        ret.raw[3][1] = ty;
        ret.raw[3][2] = tz;
        return ret;
    }

    template <typename T>
    constexpr auto m4<T>::perspective(perspectiveparams const& p) -> m4
    {
        const auto divider = 1 / (p.focal_len * (p.far - p.near));

        auto ret = m4::scaling(1, p.aspect_ratio, p.far * divider);
        ret.raw[2][3] = 1.0f / p.focal_len;
        ret.raw[3][2] = -p.far * p.near * divider;
        ret.raw[3][3] = 0;
        return ret;
    }

}
