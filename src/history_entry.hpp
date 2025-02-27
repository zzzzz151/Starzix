// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "search_params.hpp"

MAYBE_CONSTEXPR void updateHistory(i16* history, i32 bonus)
{
    assert(abs(*history) <= HISTORY_MAX);

    bonus = std::clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
    *history += bonus - abs(bonus) * i32(*history) / HISTORY_MAX;

    assert(abs(*history) <= HISTORY_MAX);
}

constexpr void updateHistory(i16 &history, const i32 bonus) {
    updateHistory(&history, bonus);
}

struct HistoryEntry {
    private:

    MultiArray<i16, 2, 2>  mMainHist  = { }; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist  = { }; // [previousMovePieceType][previousMoveTo]
    MultiArray<i16, 7, 5>  mNoisyHist = { }; // [pieceTypeCaptured][promotionPieceType]

    public:

    i16 mCorrHist = 0;

    constexpr i32 quietHistory(
        const bool enemyAttacksOrigin, const bool enemyAttacksDst, const std::array<Move, 3> moves) const
    {
        #if defined(TUNE)
            CONT_HIST_WEIGHTS = {
                contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight()
            };
        #endif

        float total = float(mMainHist[enemyAttacksOrigin][enemyAttacksDst]) * mainHistoryWeight();

        Move move;
        for (size_t i = 0; i < moves.size(); i++)
            if ((move = moves[i]) != MOVE_NONE)
            {
                const int pt = int(move.pieceType());
                total += float(mContHist[pt][move.to()]) * CONT_HIST_WEIGHTS[i];
            }

        return total;
    }

    constexpr void updateQuietHistories(
        const bool enemyAttacksOrigin, const bool enemyAttacksDst, const std::array<Move, 3> moves, const i32 bonus)
    {
        updateHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus);

        for (const Move move : moves)
            if (move != MOVE_NONE) {
                const int pt = int(move.pieceType());
                updateHistory(mContHist[pt][move.to()], bonus);
            }
    }

    constexpr i16* noisyHistoryPtr(const PieceType captured, const PieceType promotion)
    {
        const int secondIdx = promotion == PieceType::NONE ? 0 : (int)promotion - (int)PieceType::KNIGHT + 1;
        return &mNoisyHist[(int)captured][secondIdx];
    }

    constexpr i32 noisyHistory(const PieceType captured, const PieceType promotion)
    {
        return *noisyHistoryPtr(captured, promotion);
    }

}; // HistoryEntry
