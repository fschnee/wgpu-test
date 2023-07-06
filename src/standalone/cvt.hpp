// General utilities and shorthands for converting between types.
//
// The types/constants defined here only do their conversions
// when an arithmetic operation is exercized upon them
// or operator() is explictly called.
//
// You can use them as math constants OR functions.
//
// E.g: `5.8f * cvt::to<int>`     will return `int{5}`.
// E.g: `int x = 5.8f * cvt::toe` will compile and `x == 5`.
// E.g: `cvt::to<int>(5.8f)`      will return `int{5}`.
//
// The reason for this arithmetic-based interface is that you will
// usually use these conversions when dealing with numbers so it
// made the most ergonomic sense to encode them as math operations.
//
// Keep in mind that arithmetic precedence still aplies, that is,
// multiplications will be executed before additions and so on.
// E.g:
//     5.2f + cvt::to<int> * 5.99 == 10.2f
//     5.2f * cvt::to<int> + 5.99 == 10.99
#pragma once

#include "forward.hpp"

namespace standalone::cvt
{
    namespace converters
    {
        // Non-special versions of the *_cast 'functions'.

        template <typename To> struct sc; // static_cast.
        template <typename To> struct dc; // dynamic_cast.
        template <typename To> struct rc; // reinterpret_cast.
        template <typename To> struct cc; // const_cast.

        // Custom converters.

        // Uses magic to 'guess' the expected type and convert to it.
        // Can only be used in situations where the compiler can
        // infer the final type.
        struct expected;
    }

    // Just a wrapper around a Converter, you will never use this directly.
    // Provides all the arithmetic operators.
    //
    // This class is really simple, has only 1 function and that is operator(),
    // you can call it to convert the argument using Converter.
    // The magic is that all the arithmetic operations are configured to perform
    // this conversion, they are defined below, in the implementation section.
    template <typename Converter>
    struct into
    {
        template <typename From>
        constexpr auto operator()(From&& from) const;
    };

    // You can use the following like constants or functions.
    // E.g: `5.8f * cvt::to<double>` or `cvt::to<double>(5.8f)`.

    template <typename To>
    inline constexpr auto to = into< converters::sc<To> >{};
    template <typename To>
    inline constexpr auto sc = into< converters::sc<To> >{};
    template <typename To>
    inline constexpr auto dc = into< converters::dc<To> >{};
    template <typename To>
    inline constexpr auto rc = into< converters::rc<To> >{};
    template <typename To>
    inline constexpr auto cc = into< converters::cc<To> >{};

    inline constexpr auto to_expected  = into<converters::expected>{};
    inline constexpr auto toe          = into<converters::expected>{};

    // Easy way to make your own converter using a function
    // E.g:
    //    `
    constexpr auto custom(auto&& func);
}

// Implementations below.

namespace standalone::cvt
{
    template <typename Converter>
    template <typename From>
    constexpr auto into<Converter>::operator()(From&& from) const
    { return Converter{}( forward<From>(from) ); }

    // operator*
    template <typename T, typename Converter>
    constexpr auto operator*(T&& val, const into<Converter>& cvt)
    { return cvt( forward<T>(val) ); }
    template <typename T, typename Converter>
    constexpr auto operator*(const into<Converter>& cvt, T&& val)
    { return cvt( forward<T>(val) ); }

    // operator/
    template <typename T, typename Converter>
    constexpr auto operator/(T&& val, const into<Converter>& cvt)
    { return cvt( forward<T>(val) ); }
    template <typename T, typename Converter>
    constexpr auto operator/(const into<Converter>& cvt, T&& val)
    { return cvt( forward<T>(val) ); }

    // operator%
    template <typename T, typename Converter>
    constexpr auto operator%(T&& val, const into<Converter>& cvt)
    { return cvt( forward<T>(val) ); }
    template <typename T, typename Converter>
    constexpr auto operator%(const into<Converter>& cvt, T&& val)
    { return cvt( forward<T>(val) ); }

    // operator+
    template <typename T, typename Converter>
    constexpr auto operator+(T&& val, const into<Converter>& cvt)
    { return cvt( forward<T>(val) ); }
    template <typename T, typename Converter>
    constexpr auto operator+(const into<Converter>& cvt, T&& val)
    { return cvt( forward<T>(val) ); }

    // operator-
    template <typename T, typename Converter>
    constexpr auto operator-(T&& val, const into<Converter>& cvt)
    { return cvt( forward<T>(val) ); }
    template <typename T, typename Converter>
    constexpr auto operator-(const into<Converter>& cvt, T&& val)
    { return cvt( forward<T>(val) ); }

    constexpr auto custom(auto&& func) { return into< decltype(func) >{}; }
}

namespace standalone::cvt::converters
{
    template <typename To>
    struct sc
    {
        template <typename From>
        constexpr auto operator()(From&& from) const
        { return static_cast<To>( forward<From>(from) ); }
    };

    template <typename To>
    struct dc
    {
        template <typename From>
        constexpr auto operator()(From&& from) const
        { return dynamic_cast<To>( forward<From>(from) ); }
    };

    template <typename To>
    struct rc
    {
        template <typename From>
        constexpr auto operator()(From&& from) const
        { return reinterpret_cast<To>( forward<From>(from) ); }
    };

    template <typename To>
    struct cc
    {
        template <typename From>
        constexpr auto operator()(From&& from) const
        { return const_cast<To>( forward<From>(from) ); }
    };

    struct expected
    {
        // Magic happens here!
        // This type is implictly convertible to anything and
        // the compiler fills in the blanks with the correct type.
        template <typename From>
        struct implicit_cvt
        {
            From v;

            template<typename ExpectedType>
            constexpr operator ExpectedType() const &
            { return cvt::to<ExpectedType>(v); }

            template <typename ExpectedType>
            constexpr operator ExpectedType() &&
            { return cvt::to<ExpectedType>(move(v)); }
        };

        template <typename From>
        constexpr auto operator()(From&& from) const
        { return implicit_cvt<From>{ forward<From>(from) }; }
    };
}
