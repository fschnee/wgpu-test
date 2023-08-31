#pragma once

#include "aliases.hpp"

namespace ghuva::inline utils
{
    // Assumes [data, data+size] has elements of type T
    // and [data+size, data+capacity] has uninitialized memory.
    template <typename T>
    struct container
    {
        T* data = nullptr;
        u64 size = 0;
        u64 capacity = 0;

        constexpr container(T* d, u64 s, u64 c) : data{d}, size{s}, capacity{c} {}

        constexpr container() = default;
        constexpr container(container const&) = default;
        constexpr container(container&&) noexcept = default;
        constexpr auto operator=(container const&) -> container& = default;
        constexpr auto operator=(container&&) noexcept -> container& = default;

        constexpr auto begin() -> T* { return data; }
        constexpr auto end()   -> T* { return data + size; }
        constexpr auto begin() const -> T const* { return data; }
        constexpr auto end()   const -> T const* { return data + size; }

        constexpr auto byte_size()     const -> u64 { return size     * sizeof(T); }
        constexpr auto byte_capacity() const -> u64 { return capacity * sizeof(T); }
    };
}