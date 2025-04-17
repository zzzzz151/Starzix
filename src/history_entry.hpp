// clang-format off

#pragma once

#include "utils.hpp"
#include "search_params.hpp"

constexpr void updateHistory(i16* historyPtr, i32 bonus)
{
    if (historyPtr == nullptr) return;

    assert(std::abs(*historyPtr) <= HISTORY_MAX);

    bonus = std::clamp<i32>(bonus, -HISTORY_MAX, HISTORY_MAX);

    *historyPtr += static_cast<i16>(
        bonus - std::abs(bonus) * static_cast<i32>(*historyPtr) / HISTORY_MAX
    );

    assert(std::abs(*historyPtr) <= HISTORY_MAX);
}

struct HistoryEntry
{
private:

    MultiArray<i16, 2, 2> mMainHist = { }; // [enemyAttacksSrc][enemyAttacksDst]

    EnumArray<i16, Color, PieceType, Square> mContHist = { };

    EnumArray<i16, PieceType, PieceType> mNoisyHist = { }; // [captured][promotion] (King if none)

public:

    i16 mCorrHist = 0;

    EnumArray<i16, PieceType, Square> mContCorrHist = { };

    // Main history + 1-ply cont hist + 2-ply cont hist
    constexpr i32 quietHistory(Position& pos, const Move move) const
    {
        const bool enemyAttacksSrc = hasSquare(pos.enemyAttacksNoStmKing(), move.from());
        const bool enemyAttacksDst = hasSquare(pos.enemyAttacksNoStmKing(), move.to());

        const Move prevMove  = pos.lastMove();
        const Move prevMove2 = pos.nthToLastMove(2);

        i32 total = mMainHist[enemyAttacksSrc][enemyAttacksDst];

        if (prevMove)
            total += mContHist[!pos.sideToMove()][prevMove.pieceType()][prevMove.to()];

        if (prevMove2)
            total += mContHist[pos.sideToMove()][prevMove2.pieceType()][prevMove2.to()];

        return total;
    }

    // Update main history, 1-ply cont hist and 2-ply cont hist
    constexpr void updateMainHistContHist(Position& pos, const Move move, const i32 bonus)
    {
        const bool enemyAttacksSrc = hasSquare(pos.enemyAttacksNoStmKing(), move.from());
        const bool enemyAttacksDst = hasSquare(pos.enemyAttacksNoStmKing(), move.to());

        const Color stm = pos.sideToMove();

        const Move prevMove  = pos.lastMove();
        const Move prevMove2 = pos.nthToLastMove(2);

        updateHistory(&mMainHist[enemyAttacksSrc][enemyAttacksDst], bonus);

        if (prevMove)
            updateHistory(&mContHist[!stm][prevMove.pieceType()][prevMove.to()], bonus);

        if (prevMove2)
            updateHistory(&mContHist[stm][prevMove2.pieceType()][prevMove2.to()], bonus);
    }

    constexpr i32 noisyHistory(
        const std::optional<PieceType> captured, const std::optional<PieceType> promotion) const
    {
        return mNoisyHist[captured.value_or(PieceType::King)][promotion.value_or(PieceType::King)];
    }

    constexpr void updateNoisyHistory(
        const std::optional<PieceType> captured,
        const std::optional<PieceType> promotion,
        const i32 bonus)
    {
        i16& noisyHist
            = mNoisyHist[captured.value_or(PieceType::King)][promotion.value_or(PieceType::King)];

        updateHistory(&noisyHist, bonus);
    }

}; // struct HistoryEntry

// [stm][pieceType][dst]
using HistoryTable = EnumArray<HistoryEntry, Color, PieceType, Square>;
