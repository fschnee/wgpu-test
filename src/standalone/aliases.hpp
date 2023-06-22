// Who likes long types?
#pragma once

namespace standalone::inline integer_aliases
{
    namespace detail
    {
        template <auto...>
        struct false_type { static constexpr bool value = false; };

        template<int Size>
        consteval auto unsigned_integer_sized()
        {
            constexpr auto size = Size/8;
            if      constexpr(sizeof(unsigned char)      == size) { return static_cast<unsigned char>(0); }
            else if constexpr(sizeof(unsigned short)     == size) { return static_cast<unsigned short>(0); }
            else if constexpr(sizeof(unsigned int)       == size) { return static_cast<unsigned int>(0); }
            else if constexpr(sizeof(unsigned long)      == size) { return static_cast<unsigned long>(0); }
            else if constexpr(sizeof(unsigned long long) == size) { return static_cast<unsigned long long>(0); }
            else { static_assert(false_type<Size>::value, "No type of this size"); }
        }

        template<int Size>
        consteval auto signed_integer_sized()
        {
            constexpr auto size = Size/8;
            if      constexpr(sizeof(signed char)      == size) { return static_cast<signed char>(0); }
            else if constexpr(sizeof(signed short)     == size) { return static_cast<signed short>(0); }
            else if constexpr(sizeof(signed int)       == size) { return                           0; }
            else if constexpr(sizeof(signed long)      == size) { return static_cast<signed long>(0); }
            else if constexpr(sizeof(signed long long) == size) { return static_cast<signed long long>(0); }
            else { static_assert(false_type<Size>::value, "No type of this size"); }
        }
    };

    using u8  = decltype(detail::unsigned_integer_sized<8>());
    using u16 = decltype(detail::unsigned_integer_sized<16>());
    using u32 = decltype(detail::unsigned_integer_sized<32>());
    using u64 = decltype(detail::unsigned_integer_sized<64>());

    using i8  = decltype(detail::signed_integer_sized<8>());
    using i16 = decltype(detail::signed_integer_sized<16>());
    using i32 = decltype(detail::signed_integer_sized<32>());
    using i64 = decltype(detail::signed_integer_sized<64>());

    inline namespace literals
    {
        constexpr auto operator""_u8(unsigned long long val)  { return static_cast<u8>(val); }
        constexpr auto operator""_u16(unsigned long long val) { return static_cast<u16>(val); }
        constexpr auto operator""_u32(unsigned long long val) { return static_cast<u32>(val); }
        constexpr auto operator""_u64(unsigned long long val) { return static_cast<u64>(val); }

        constexpr auto operator""_i8(unsigned long long val)  { return static_cast<i8>(val); }
        constexpr auto operator""_i16(unsigned long long val) { return static_cast<i16>(val); }
        constexpr auto operator""_i32(unsigned long long val) { return static_cast<i32>(val); }
        constexpr auto operator""_i64(unsigned long long val) { return static_cast<i64>(val); }
    };
}

namespace s = standalone;
