// clang-format off

#pragma once

#include "utils.hpp"
#include "search_params.hpp"

constexpr void updateHistory(i16& history, i32 bonus)
{
    assert(std::abs(history) <= HISTORY_MAX);

    bonus = std::clamp<i32>(bonus, -HISTORY_MAX, HISTORY_MAX);

    history += static_cast<i16>(
        bonus - std::abs(bonus) * static_cast<i32>(history) / HISTORY_MAX
    );

    assert(std::abs(history) <= HISTORY_MAX);
}

struct HistoryEntry
{
private:

    i16 mMainHist = 0;

public:

    constexpr i32 getHistory() const {
        return static_cast<i32>(mMainHist);
    }

    constexpr void update(const i32 bonus)
    {
        updateHistory(mMainHist, bonus);
    }

}; // struct HistoryEntry
