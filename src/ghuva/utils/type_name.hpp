#pragma once

#include <type_traits>
#include <string_view>

#include "forward.hpp"
#include "aliases.hpp"

namespace ghuva::inline utils
{
    // from https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138
    // Thanks!
    template<typename T, bool EnableShortInts = true>
    constexpr auto type_name() -> std::string_view
    {
        if constexpr (EnableShortInts && std::is_integral_v<T> && sizeof(T) <= sizeof(u64))
        {
            constexpr const char* unsigned_atlas[] = {"u8", "u16", "dummy", "u32", "dummy", "dummy", "dummy", "u64"};
            constexpr const char* signed_atlas[]   = {"i8", "i16", "dummy", "i32", "dummy", "dummy", "dummy", "i64"};

            if constexpr (std::is_signed_v<T>){return signed_atlas[sizeof(T) - 1];}
            else return unsigned_atlas[sizeof(T) - 1];
        }
        std::string_view name, prefix, suffix;
        #if defined(__clang__)
            name   = __PRETTY_FUNCTION__;
            prefix = "std::string_view ghuva::type_name() [T = ";
            if constexpr (EnableShortInts) { suffix = ", EnableShortInts = true]"; }
            else                           { suffix = ", EnableShortInts = false]"; }
        #elif defined(__GNUC__)
            name   = __PRETTY_FUNCTION__;
            prefix = "constexpr std::string_view ghuva::utils::type_name() [with T = ";
            if constexpr (EnableShortInts) { suffix = "; bool EnableShortInts = true; std::string_view = std::basic_string_view<char>]"; }
            else                           { suffix = "; bool EnableShortInts = false; std::string_view = std::basic_string_view<char>]"; }
        #elif defined(_MSC_VER)
            name = __FUNCSIG__;
            prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ghuva::utils::type_name<";
            if constexpr(EnableShortInts) { suffix = ",true>(void)"; }
            else                          { suffix = ",false>(void)"; }
        #else
            static_assert(false, "Please implement type_name() for this compiler");
        #endif
        name.remove_prefix(prefix.size());
        name.remove_suffix(suffix.size());
        return name;
    }

    template <typename T, bool EnableShortInts = true>
    constexpr auto type_name(T&&) -> std::string_view
    { return type_name< T, EnableShortInts >(); }

    template <typename T, bool EnableShortInts = true>
    constexpr auto clean_type_name(T&&) -> std::string_view
    { return type_name< remove_cvref_t<T>, EnableShortInts >(); }
}
