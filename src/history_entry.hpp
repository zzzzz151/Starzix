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

    // Main history, 1-ply cont hist, 2-ply cont hist
    constexpr const std::array<const i16*, 3> quietHistoriesPtrs(
        Position& pos, const Move move) const
    {
        const Bitboard enemyAttacks = pos.enemyAttacksNoStmKing();

        const bool enemyAttacksSrc = hasSquare(enemyAttacks, move.from());
        const bool enemyAttacksDst = hasSquare(enemyAttacks, move.to());

        std::array<const i16*, 3> histories = {
            &mMainHist[enemyAttacksSrc][enemyAttacksDst], nullptr, nullptr
        };

        // 1-ply cont hist
        Move prevMove = pos.lastMove();
        if (prevMove)
            histories[1] = &mContHist[!pos.sideToMove()][prevMove.pieceType()][prevMove.to()];

        // 2-ply cont hist
        prevMove = pos.nthToLastMove(2);
        if (prevMove)
            histories[2] = &mContHist[pos.sideToMove()][prevMove.pieceType()][prevMove.to()];

        return histories;
    }

public:

    i16 mCorrHist = 0;

    constexpr i32 quietHistory(Position& pos, const Move move) const
    {
        i32 total = 0;

        for (const i16* historyPtr : quietHistoriesPtrs(pos, move))
            if (historyPtr != nullptr)
                total += static_cast<i32>(*historyPtr);

        return total;
    }

    constexpr void updateQuietHistories(Position& pos, const Move move, const i32 bonus)
    {
        for (const i16* historyPtr : quietHistoriesPtrs(pos, move))
            updateHistory(const_cast<i16*>(historyPtr), bonus);
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

// [stm][pieceType][targetSquare]
using HistoryTable = EnumArray<HistoryEntry, Color, PieceType, Square>;
