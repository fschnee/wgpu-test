#pragma once

#include "forward.hpp"

namespace ghuva
{
    template <typename Func>
    struct dont_forget
    {
        constexpr dont_forget() = default;
        constexpr dont_forget(Func&& oe) : on_exit{ forward<Func>(oe) } {}

        // No copying, just moving (>:O)
        dont_forget(const dont_forget&) = delete;
        auto operator=(const dont_forget&) -> dont_forget& = delete;

        constexpr dont_forget(dont_forget&& other) { *this = move(other); }
        constexpr auto operator=(dont_forget&& other) -> dont_forget&
        {
            this->on_exit = move(other.on_exit);
            this->engaged = other.engaged;
            other.clear();
        }

        constexpr auto is_engaged() -> bool { return this->engaged; }
        constexpr auto clear() -> dont_forget&
        {
            this->engaged = false;
        }

        ~dont_forget() { if(engaged) on_exit(); }

    private:
        bool engaged = false;
        Func on_exit = []{};
    };
}

#define _STANDALONE_DONT_FORGET_CONCAT_IMPL(x, y) x##y
#define _STANDALONE_DONT_FORGET_CONCAT(x, y) _STANDALONE_DONT_FORGET_CONCAT_IMPL(x, y)

#define STANDALONE_DONT_FORGET( body ) \
    auto _STANDALONE_DONT_FORGET_CONCAT(STANDALONE_reminder_on_line_, __LINE__) = ghuva::dont_forget{ [&]{ body; }}
