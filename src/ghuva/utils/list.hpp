#pragma once

#include "forward.hpp"
#include "container.hpp"
#include "aliases.hpp"

// Both <initializer_list> and <new> are quite
// fast to include and codegen so it should be fine.
#include <initializer_list>
#include <new>

namespace ghuva::inline utils
{
    template <typename T>
    class list
    {
    public:
        constexpr list() = default;
        constexpr list(u64);

        // Copy constructors.
        constexpr list(list const&);
        constexpr list(T* begin, T* end); // Assumes begin != end.
        static constexpr auto from_container(container<T> const&) -> list; // TODO

        // Move constructors.
        constexpr list(list&&) noexcept;
        static constexpr auto from_container(container<T>&&) noexcept -> list;

        // Elementwise move constructors.
        constexpr list(std::initializer_list<T>);

        ~list() { clear(); if(elements.data) delete elements.data; }

        constexpr auto operator=(list const&) -> list&;
        constexpr auto operator=(list&&) noexcept -> list&;

        constexpr auto operator[](u64)       -> T&;
        constexpr auto operator[](u64) const -> T const&;

        [[nodiscard]] constexpr auto size()     const -> u64;
        // Only use if you know what you are doing.
        // Useful if you are using this as resizable buffer
        // that is filled by someone else.
        constexpr auto override_size(u64)             -> list&;
        [[nodiscard]] constexpr auto capacity() const -> u64;

        [[nodiscard]] constexpr auto begin() -> T*;
        [[nodiscard]] constexpr auto end()   -> T*;
        [[nodiscard]] constexpr auto begin() const -> T const*;
        [[nodiscard]] constexpr auto end()   const -> T const*;

        constexpr auto reserve(u64) -> list&;
        constexpr auto reserve_nocopy(u64) -> list&;

        constexpr auto push_back(T const&)     -> list&;
        constexpr auto push_back(T&&)          -> list&;
        template <typename... Args>
        constexpr auto emplace_back(Args&&...) -> list&;

        [[nodiscard]] constexpr auto surrender() noexcept -> container<T>;

        // Capacity remains unchanged.
        constexpr auto clear() -> list&;

    private:
        container<T> elements = {nullptr, 0, 0};
    };
}

template <typename T>
constexpr ghuva::list<T>::list(u64 size)
{
    elements.data = static_cast<T*>(::operator new(size * sizeof(T)));
    elements.capacity = size;
}

template <typename T>
constexpr ghuva::list<T>::list(const list& other)
{ *this = other; }

template <typename T>
constexpr ghuva::list<T>::list(T* begin, T* end)
{
    reserve( static_cast<u64>(end - begin + 1) );
    for(auto e = begin; e != end; ++e) push_back(*e);
}

template <typename T>
constexpr auto ghuva::list<T>::from_container(container<T> const& container) -> list
{
    auto l = list(container.size);
    for(auto i = 0_u64; i < container.size; ++i) l.push_back(container.data[i]);
    return l;
}

template <typename T>
constexpr ghuva::list<T>::list(list&& other) noexcept
{ *this = ghuva::move(other); }

template <typename T>
constexpr auto ghuva::list<T>::from_container(container<T>&& container) noexcept -> list
{
    auto l = list();
    l.elements = ghuva::move(container);
    container = {nullptr, 0, 0};
    return l;
}

template <typename T>
constexpr ghuva::list<T>::list(std::initializer_list<T> l)
{
    reserve( l.size() );
    for(auto& e : l) push_back( ghuva::move(e) );
}

template <typename T>
constexpr auto ghuva::list<T>::operator=(const list& other) -> list&
{
    clear();
    reserve(other.elements.size);

    for(auto const& e : other) push_back(e);

    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::operator=(list&& other) noexcept -> list&
{
    // Is actually a swap but thats okay.
    auto temp = ghuva::move(elements);
    elements = ghuva::move(other.elements);
    other.elements = ghuva::move(temp);

    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::operator[](u64 pos) -> T&
{ return elements.data[pos]; }

template <typename T>
constexpr auto ghuva::list<T>::operator[](u64 pos) const -> T const&
{ return elements.data[pos]; }

template <typename T>
constexpr auto ghuva::list<T>::size() const -> u64
{ return elements.size; }

template <typename T>
constexpr auto ghuva::list<T>::override_size(u64 newsize) -> list&
{ elements.size = newsize; return *this; }

template <typename T>
constexpr auto ghuva::list<T>::capacity() const -> u64
{ return elements.capacity; }

template <typename T>
constexpr auto ghuva::list<T>::begin() -> T*
{ return elements.data; }

template <typename T>
constexpr auto ghuva::list<T>::end() -> T*
{ return elements.data + elements.size; }

template <typename T>
constexpr auto ghuva::list<T>::begin() const -> T const*
{ return elements.data; }

template <typename T>
constexpr auto ghuva::list<T>::end() const -> T const*
{ return elements.data + elements.size; }

template <typename T>
constexpr auto ghuva::list<T>::reserve(u64 new_size) -> list&
{
    if(new_size <= elements.capacity) return *this;

    auto new_list = list(new_size);
    for(auto& e : *this) new_list.push_back( ghuva::move(e) );
    *this = ghuva::move(new_list);

    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::reserve_nocopy(u64 new_size) -> list&
{
    if(new_size <= elements.capacity) return *this;

    *this = list(new_size);

    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::push_back(T const& new_element) -> list&
{
    if(elements.size == elements.capacity) reserve(elements.capacity ? elements.capacity * 2 : 1);
    auto e = new ( elements.data + elements.size++ ) T;
    *e = new_element;
    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::push_back(T&& new_element) -> list&
{
    if(elements.size == elements.capacity) reserve(elements.capacity ? elements.capacity * 2 : 1);
    auto e = new ( elements.data + elements.size++ ) T;
    *e = ghuva::move(new_element);
    return *this;
}

template <typename T>
template <typename... Args>
constexpr auto ghuva::list<T>::emplace_back(Args&&... args) -> list&
{
    if(elements.size == elements.capacity) reserve(elements.capacity ? elements.capacity * 2 : 1);
    new ( elements.data + elements.size++ ) T( ghuva::forward<Args>(args)... );
    return *this;
}

template <typename T>
constexpr auto ghuva::list<T>::surrender() noexcept -> container<T>
{
    auto const ret = elements;
    elements = {nullptr, 0, 0};
    return ret;
}

template <typename T>
constexpr auto ghuva::list<T>::clear() -> list&
{
    for(auto& e : *this) e.~T();
    elements.size = 0;
    return *this;
}
