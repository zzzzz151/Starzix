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
    EnumArray<i16, PieceType, Square> mContHist = { };

public:

    constexpr i32 getHistory(const Move lastMove) const
    {
        i32 total = static_cast<i32>(mMainHist);

        if (lastMove != MOVE_NONE)
            total += static_cast<i32>(mContHist[lastMove.pieceType()][lastMove.to()]);

        return total;
    }

    constexpr void update(const i32 bonus, const Move lastMove)
    {
        updateHistory(mMainHist, bonus);

        if (lastMove != MOVE_NONE)
            updateHistory(mContHist[lastMove.pieceType()][lastMove.to()], bonus);
    }

}; // struct HistoryEntry
