// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "history_entry.hpp"

struct ScoredMove
{
public:
    Move move;
    i32 score;
};

// Incremental selection sort
constexpr ScoredMove nextBest(ArrayVec<ScoredMove, 256>& scoredMoves)
{
    if (scoredMoves.size() == 0)
        return ScoredMove{ .move = MOVE_NONE, .score = 0 };

    size_t bestIdx = 0;

    for (size_t i = 1; i < scoredMoves.size(); i++)
        if (scoredMoves[i].score > scoredMoves[bestIdx].score)
            bestIdx = i;

    const ScoredMove best = scoredMoves[bestIdx];

    // Swap best ScoredMove to the end and pop it
    scoredMoves.swap(bestIdx, scoredMoves.size() - 1);
    scoredMoves.pop_back();

    return best;
}

class MovePicker
{
private:

    i32 mStage = 0;

    bool mNoisiesOnly = false;

    Move mTtMove = MOVE_NONE;
    Move mKiller = MOVE_NONE;
    Move mExcludedMove = MOVE_NONE;

    ArrayVec<ScoredMove, 256> mNoisies, mQuiets;

    ScoredMove mFirstBadNoisy = { .move = MOVE_NONE, .score = 0 };

public:

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

    constexpr ScoredMove nextLegal(Position& pos, const HistoryTable& historyTable)
    {
        switch (mStage)
        {
        // TT move
        case 0:
        {
            mStage++;

            if (mTtMove
            && mTtMove != mExcludedMove
            && (!mNoisiesOnly || !pos.isQuiet(mTtMove))
            && isPseudolegal(pos, mTtMove)
            && isPseudolegalLegal(pos, mTtMove))
                return ScoredMove{ .move = mTtMove, .score = 0 };

            return nextLegal(pos, historyTable);
        }
        // Generate and score noisy moves
        case 1:
        {
            for (const Move move : pseudolegalMoves<MoveGenType::NoisyOnly>(pos))
                if (move != mTtMove && move != mExcludedMove)
                {
                    const i32 score = scoreNoisy(move, pos, historyTable);
                    mNoisies.push_back(ScoredMove{ .move = move, .score = score });
                }

            mStage++;
            return nextLegal(pos, historyTable);
        }
        // Yield good noisy moves
        case 2:
        {
            const ScoredMove scoredMove = nextBest(mNoisies);

            if (!scoredMove.move || scoredMove.score < 0)
            {
                // Save the best bad noisy move for the bad noisy moves stage
                if (scoredMove.move) mFirstBadNoisy = scoredMove;

                // If only noisy moves, start yielding bad noisy moves
                // Otherwise, go on to the quiet moves
                mStage = mNoisiesOnly ? 6 : mStage + 1;
            }
            else if (isPseudolegalLegal(pos, scoredMove.move))
                return scoredMove;

            return nextLegal(pos, historyTable);
        }
        // Killer move (quiet)
        case 3:
        {
            mStage++;

            if (mKiller
            && mKiller != mTtMove
            && mKiller != mExcludedMove
            && pos.isQuiet(mKiller)
            && isPseudolegal(pos, mKiller)
            && isPseudolegalLegal(pos, mKiller))
                return ScoredMove{ .move = mKiller, .score = 0 };

            return nextLegal(pos, historyTable);
        }
        // Generate and score quiet moves
        case 4:
        {
            for (const Move move : pseudolegalMoves<MoveGenType::QuietOnly>(pos))
            {
                if (move == mTtMove || move == mKiller || move == mExcludedMove)
                    continue;

                const HistoryEntry& histEntry
                    = historyTable[pos.sideToMove()][move.pieceType()][move.to()];

                const i32 quietHist = histEntry.quietHistory(pos, move);

                mQuiets.push_back(ScoredMove{ .move = move, .score = quietHist });
            }

            mStage++;
            return nextLegal(pos, historyTable);
        }
        // Yield quiet moves
        case 5:
        {
            const ScoredMove scoredMove = nextBest(mQuiets);

            if (!scoredMove.move)
                mStage++;
            else if (isPseudolegalLegal(pos, scoredMove.move))
                return scoredMove;

            return nextLegal(pos, historyTable);
        }
        // Yield bad noisy moves
        case 6:
        {
            // We may already have the best bad noisy move
            // (from the ending of the good noisy moves stage)
            if (mFirstBadNoisy.move)
            {
                const ScoredMove firstBadNoisy = mFirstBadNoisy;
                mFirstBadNoisy = ScoredMove{ .move = MOVE_NONE, .score = 0 };

                if (isPseudolegalLegal(pos, firstBadNoisy.move))
                    return firstBadNoisy;
            }

            const ScoredMove scoredMove = nextBest(mNoisies);

            if (!scoredMove.move)
                mStage++;
            else if (isPseudolegalLegal(pos, scoredMove.move))
                return scoredMove;

            return nextLegal(pos, historyTable);
        }
        // No more legal moves
        default:
        {
            return ScoredMove{ .move = MOVE_NONE, .score = 0 };
        }
        } // switch (mStage)
    }

    constexpr i32 scoreNoisy(
        const Move move, const Position& pos, const HistoryTable& historyTable)
    {
        i32 score = 0;

        const std::optional<PieceType> captured = pos.captured(move);
        const std::optional<PieceType> promo    = move.promotion();

        constexpr EnumArray<std::optional<i32>, PieceType> PROMO_SCORE = {
            std::nullopt, // P
            -3, // N
            -5, // B
            -4, // R
            2,  // Q
            std::nullopt // K
        };

        if (mNoisiesOnly)
            score = promo.has_value() ? PROMO_SCORE[*promo].value() : 1;
        else if (promo.has_value())
        {
            score = promo == PieceType::Queen
                  ? (pos.SEE(move) ? 3 : -1)
                  : PROMO_SCORE[*promo].value();
        }
        else {
            const HistoryEntry& histEntry
                = historyTable[pos.sideToMove()][move.pieceType()][move.to()];

            const i32 noisyHist = histEntry.noisyHistory(captured, promo);

            const i32 threshold = static_cast<i32>(lround(
                static_cast<float>(-noisyHist) * seeNoisyHistMul()
            ));

            score = 2 * (pos.SEE(move, threshold) ? 1 : -1);
        }

        score *= 10'000;

        // MVVLVA (most valuable victim, least valuable attacker)

        const i32 iCaptured = captured.has_value() ? static_cast<i32>(*captured) + 1 : 0;

        const PieceType pt = move.pieceType();
        const i32 iPieceType = pt == PieceType::King ? 0 : static_cast<i32>(pt) + 1;

        return score + iCaptured * 100 - iPieceType;
    }

}; // class MovePicker
