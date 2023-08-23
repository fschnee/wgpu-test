#pragma once

#include <chrono>

namespace ghuva::inline chrono
{
    template<
        typename Timer    = std::chrono::high_resolution_clock,
        typename Duration = std::chrono::duration<float>,
        typename Functor
    >
    inline auto time(Functor&& f)
    {
        auto t1 = Timer::now();
        f();
        auto t2 = Timer::now();
        return std::chrono::duration_cast<Duration>(t2 - t1).count();
    }

    template <typename Timer = std::chrono::high_resolution_clock>
    struct stopwatch
    {
        using timer = Timer;
        using time_point = decltype(timer::now());

        constexpr stopwatch();

        constexpr auto restart() -> stopwatch&;
        constexpr auto click() -> stopwatch&;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto last_segment() const;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto since_click() const;

        template <typename Duration = std::chrono::duration<float>>
        constexpr auto since_beginning() const;

    private:
        time_point start;
        time_point last_segment_start;
        time_point this_segment_start;
    };
}

// Implementations.

template <typename Timer>
constexpr ghuva::chrono::stopwatch<Timer>::stopwatch()
    : start{timer::now()}
    , last_segment_start{start}
    , this_segment_start{start}
{}

template <typename Timer>
constexpr auto ghuva::chrono::stopwatch<Timer>::restart() -> stopwatch&
{ return (*this = stopwatch()); }

template <typename Timer>
constexpr auto ghuva::chrono::stopwatch<Timer>::click() -> stopwatch&
{
    last_segment_start = this_segment_start;
    this_segment_start = timer::now();
    return *this;
}

template <typename Timer>
template <typename Duration>
constexpr auto ghuva::chrono::stopwatch<Timer>::last_segment() const
{ return std::chrono::duration_cast<Duration>(this_segment_start - last_segment_start).count(); }

template <typename Timer>
template <typename Duration>
constexpr auto ghuva::chrono::stopwatch<Timer>::since_click() const
{ return std::chrono::duration_cast<Duration>(timer::now() - this_segment_start).count(); }

template <typename Timer>
template <typename Duration>
constexpr auto ghuva::chrono::stopwatch<Timer>::since_beginning() const
{ return std::chrono::duration_cast<Duration>(timer::now() - start).count(); }
