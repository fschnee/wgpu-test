#pragma once

#include <shared_mutex>
#include <mutex>

namespace ghuva::inline utils
{
    // Very simple wrapper for concurrent resources.
    template <typename T>
    struct guarded
    {
        auto write(auto f);
        auto read(auto f) const;

    private:
        T data;
        mutable std::shared_mutex mutex;
    };
}

// Impls.

template <typename T>
auto ghuva::utils::guarded<T>::write(auto f)
{
    std::unique_lock lock(mutex);
    f(data);
}

template <typename T>
auto ghuva::utils::guarded<T>::read(auto f) const
{
    std::shared_lock lock(mutex);
    return f(data);
}
