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

    MultiArray<i16, 2, 2> mMainHist = { }; // [enemyAttacksSrc][enemyAttacksDst]

    EnumArray<i16, PieceType, Square> mContHist = { };

    MultiArray<i16, 7, 5> mNoisyHist = { }; // [pieceTypeCaptured][promotionPieceType]

public:

    constexpr i32 quietHistory(
        const bool enemyAttacksSrc, const bool enemyAttacksDst, const Move lastMove) const
    {
        i32 total = static_cast<i32>(mMainHist[enemyAttacksSrc][enemyAttacksDst]);

        if (lastMove != MOVE_NONE)
            total += static_cast<i32>(mContHist[lastMove.pieceType()][lastMove.to()]);

        return total;
    }

    constexpr void updateQuietHistories(
        const bool enemyAttacksSrc,
        const bool enemyAttacksDst,
        const Move lastMove,
        const i32 bonus)
    {
        updateHistory(mMainHist[enemyAttacksSrc][enemyAttacksDst], bonus);

        if (lastMove != MOVE_NONE)
            updateHistory(mContHist[lastMove.pieceType()][lastMove.to()], bonus);
    }

private:

    constexpr std::pair<size_t, size_t> noisyHistoryIdxs(
        const std::optional<PieceType> captured, const std::optional<PieceType> promotion) const
    {
        const size_t firstIdx = captured ? static_cast<size_t>(*captured) : 6;

        const size_t secondIdx
            = promotion
            ? static_cast<size_t>(*promotion) - static_cast<size_t>(PieceType::Knight)
            : 4;

        return { firstIdx, secondIdx };
    }

public:

    constexpr i32 noisyHistory(
        const std::optional<PieceType> captured, const std::optional<PieceType> promotion) const
    {
        const auto [firstIdx, secondIdx] = noisyHistoryIdxs(captured, promotion);
        return static_cast<i32>(mNoisyHist[firstIdx][secondIdx]);
    }

    constexpr void updateNoisyHistory(
        const std::optional<PieceType> captured,
        const std::optional<PieceType> promotion,
        const i32 bonus)
    {
        const auto [firstIdx, secondIdx] = noisyHistoryIdxs(captured, promotion);
        updateHistory(mNoisyHist[firstIdx][secondIdx], bonus);
    }

}; // struct HistoryEntry

// [stm][pieceType][targetSquare]
using HistoryTable = EnumArray<HistoryEntry, Color, PieceType, Square>;
