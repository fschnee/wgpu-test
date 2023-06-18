#pragma once

#include "standalone/forward.hpp"

namespace standalone
{
    template <typename Func>
    struct dont_forget
    {
        constexpr dont_forget(Func&& oe) : on_exit{ standalone::forward<Func>(oe) } {}

        // No copying or moving (>:O)
        dont_forget() = delete;
        dont_forget(dont_forget&&) = delete;
        dont_forget(const dont_forget&) = delete;
        auto operator=(dont_forget&&) -> dont_forget& = delete;
        auto operator=(const dont_forget&) -> dont_forget& = delete;

        ~dont_forget() { on_exit(); }

    private:
        Func on_exit;
    };
}

#define _STANDALONE_DONT_FORGET_CONCAT_IMPL(x, y) x##y
#define _STANDALONE_DONT_FORGET_CONCAT(x, y) _STANDALONE_DONT_FORGET_CONCAT_IMPL(x, y)

#define STANDALONE_DONT_FORGET( body ) \
    auto _STANDALONE_DONT_FORGET_CONCAT(STANDALONE_reminder_on_line_, __LINE__) = standalone::dont_forget{ [&]{ body; }}
