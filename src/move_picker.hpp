// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "history_entry.hpp"

constexpr i32 getSEEThreshold(
    const Position& pos, const HistoryTable& historyTable, const Move move)
{
    if (move.promotion()) return 0;

    const i32 noisyHist = historyTable[pos.sideToMove()][move.pieceType()][move.to()]
        .noisyHistory(pos.captured(move), move.promotion());

    const float threshold = static_cast<float>(-noisyHist) * seeNoisyHistMul();
    return lround(threshold);
}

struct MovesData
{
public:

    ArrayVec<Move, 256> mMoves;
    std::array<i32, 256> mScores;
    std::optional<size_t> mIdx = std::nullopt;

    template <MoveGenType moveGenType>
    constexpr void genAndScoreMoves(
        Position& pos,
        const Move ttMove,
        const Move killer,
        const Move excludedMove,
        const bool doSEE,
        const HistoryTable& historyTable)
    {
        static_assert(moveGenType != MoveGenType::AllMoves);

        // Already generated and scored?
        if (mIdx) return;

        // Generate pseudolegal moves
        mMoves = pseudolegalMoves<moveGenType>(pos);

        // Score moves
        for (size_t i = 0; i < mMoves.size();)
        {
            const Move move = mMoves[i];

            if (move == ttMove || move == killer || move == excludedMove)
            {
                mMoves.swap(i, mMoves.size() - 1);
                mMoves.pop_back();
                continue;
            }

            if constexpr (moveGenType == MoveGenType::NoisyOnly)
            {
                const std::optional<PieceType> promo = move.promotion();

                if (doSEE) {
                    constexpr EnumArray<std::optional<i32>, PieceType> UNDERPROMO_SCORE = {
                    //       P           N        B        R          Q             K
                        std::nullopt, -30'000, -50'000, -40'000, std::nullopt, std::nullopt
                    };

                    const i32 threshold = getSEEThreshold(pos, historyTable, move);

                    mScores[i] = promo && *promo != PieceType::Queen
                               // Underpromotion
                               ? *(UNDERPROMO_SCORE[*promo])
                               : promo
                               // Queen promotion
                               ? (pos.SEE(move, threshold) ? 30'000 : -10'000)
                               // Capture without promotion
                               : 20'000 * (pos.SEE(move, threshold) ? 1 : -1);
                }
                else {
                    constexpr EnumArray<std::optional<i32>, PieceType> PROMO_SCORE = {
                        //   P           N        B        R       Q          K
                        std::nullopt, -30'000, -50'000, -40'000, 20'000, std::nullopt
                    };

                    mScores[i] = promo ? *(PROMO_SCORE[*promo]) : 10'000;
                }

                // MVVLVA (most valuable victim, least valuable attacker)

                const std::optional<PieceType> captured = pos.captured(move);
                const i32 iCaptured = captured ? static_cast<i32>(*captured) + 1 : 0;

                const PieceType pt = move.pieceType();
                const i32 iPieceType = pt == PieceType::King ? 0 : static_cast<i32>(pt) + 1;

                mScores[i] += iCaptured * 100 - iPieceType;
            }
            else if constexpr (moveGenType == MoveGenType::QuietOnly)
            {
                mScores[i] = historyTable[pos.sideToMove()][move.pieceType()][move.to()]
                    .quietHistory(pos, move);
            }

            i++;
        }

        mIdx = 0;
    }

    // Incremental selection sort
    constexpr std::pair<Move, i32> next()
    {
        assert(mIdx);

        if (*mIdx >= mMoves.size())
            return { MOVE_NONE, 0 };

        size_t bestIdx = *mIdx;

        for (size_t i = *mIdx + 1; i < mMoves.size(); i++)
            if (mScores[i] > mScores[bestIdx])
                bestIdx = i;

        mMoves.swap(bestIdx, *mIdx);
        std::swap(mScores[bestIdx], mScores[*mIdx]);

        const std::pair<Move, i32> moveAndScore = { mMoves[*mIdx], mScores[*mIdx] };

        (*mIdx)++;

        return moveAndScore;
    }

}; // struct MovesData

struct MovePicker
{
private:

    Move mTtMove = MOVE_NONE;
    Move mKiller = MOVE_NONE;
    Move mExcludedMove = MOVE_NONE;

    bool mTtMoveDone = false;
    bool mKillerDone = false;

    MovesData mNoisiesData, mQuietsData;

    bool mBadNoisyReady = false;

public:

    bool mNoisiesOnly = false;

    constexpr MovePicker(
        const bool noisiesOnly,
        const Move ttMove,
        const Move killer,
        const Move excludedMove = MOVE_NONE)
    {
        mNoisiesOnly = noisiesOnly;
        mTtMove = ttMove;
        mKiller = killer;
        mExcludedMove = excludedMove;
    }

    // Returns move and its score
    constexpr std::pair<Move, std::optional<i32>> nextLegal(
        Position& pos,
        const HistoryTable& historyTable)
    {
        // Maybe return TT move the first time this function is called
        if (!mTtMoveDone)
        {
            mTtMoveDone = true;

            if (mTtMove
            && mTtMove != mExcludedMove
            && (!mNoisiesOnly || !pos.isQuiet(mTtMove))
            && isPseudolegal(pos, mTtMove)
            && isPseudolegalLegal(pos, mTtMove))
                return { mTtMove, std::nullopt };
        }

        // If not done already, gen and score noisy moves
        mNoisiesData.genAndScoreMoves<MoveGenType::NoisyOnly>(
            pos, mTtMove, MOVE_NONE, mExcludedMove, !mNoisiesOnly, historyTable
        );

        // Find next noisy move
        while (!mBadNoisyReady)
        {
            const auto [move, score] = mNoisiesData.next();

            if (!move) break;

            if (score < 0) {
                mBadNoisyReady = true;
                break;
            }

            // Yield good noisy move
            if (isPseudolegalLegal(pos, move))
                return { move, score };
        }

        if (mNoisiesOnly) goto badNoisyMove;

        // Maybe return killer move
        if (!mKillerDone)
        {
            mKillerDone = true;

            if (mKiller
            && mKiller != mExcludedMove
            && pos.isQuiet(mKiller)
            && isPseudolegal(pos, mKiller)
            && isPseudolegalLegal(pos, mKiller))
                return { mKiller, std::nullopt };
        }

        // If not done already, gen and score quiet moves
        mQuietsData.genAndScoreMoves<MoveGenType::QuietOnly>(
            pos, mTtMove, mKiller, mExcludedMove, true, historyTable
        );

        // Yield quiet moves
        while (true)
        {
            const auto [move, score] = mQuietsData.next();

            if (!move) break;

            if (isPseudolegalLegal(pos, move))
                return { move, score };
        }

        badNoisyMove:

        // Yield bad noisy move
        if (mBadNoisyReady)
        {
            mBadNoisyReady = false;

            const Move move = mNoisiesData.mMoves[*(mNoisiesData.mIdx) - 1];

            if (isPseudolegalLegal(pos, move))
                return { move, mNoisiesData.mScores[*(mNoisiesData.mIdx) - 1] };

            return nextLegal(pos, historyTable);
        }

        // No more legal moves
        return { MOVE_NONE, std::nullopt };
    }

}; // struct MovePicker
