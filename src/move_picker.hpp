// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "history_entry.hpp"

enum class MoveRanking : i32 {
    TtMove = 10, GoodNoisy = 9, Killer = 8, Quiet = 7, BadNoisy = 6
};

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
        const bool doSEE,
        const EnumArray<HistoryEntry, Color, PieceType, Square>& historyTable)
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

            if (move == ttMove || move == killer)
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

                    mScores[i] = promo && *promo != PieceType::Queen
                               ? *(UNDERPROMO_SCORE[*promo])
                               : promo
                               ? (pos.SEE(move) ? 30'000 : -10'000) // Queen promo
                               : 20'000 * (pos.SEE(move) ? 1 : -1);
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
                    .getHistory(pos.lastMove());
            }

            i++;
        }

        mIdx = 0;
    }

    constexpr std::pair<Move, i32> next()
    {
        assert(mIdx);

        if (*mIdx >= mMoves.size())
            return { MOVE_NONE, 0 };

        // Incremental selection sort
        for (size_t i = *mIdx + 1; i < mMoves.size(); i++)
            if (mScores[i] > mScores[*mIdx])
            {
                mMoves.swap(i, *mIdx);
                std::swap(mScores[i], mScores[*mIdx]);
            }

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

    bool mTtMoveDone = false;
    bool mKillerDone = false;

    MovesData mNoisiesData, mQuietsData;

    bool mBadNoisyReady = false;

public:

    bool mNoisiesOnly = false;

    constexpr MovePicker(const bool noisiesOnly, const Move ttMove, const Move killer)
    {
        mNoisiesOnly = noisiesOnly;
        mTtMove = ttMove;
        mKiller = killer;
    }

    constexpr std::pair<Move, std::optional<MoveRanking>> nextLegal(
        Position& pos,
        const EnumArray<HistoryEntry, Color, PieceType, Square>& historyTable)
    {
        // Maybe return TT move the first time this function is called
        if (!mTtMoveDone)
        {
            mTtMoveDone = true;

            if (mTtMove != MOVE_NONE
            && (!mNoisiesOnly || !pos.isQuiet(mTtMove))
            && isPseudolegal(pos, mTtMove)
            && isPseudolegalLegal(pos, mTtMove))
                return { mTtMove, MoveRanking::TtMove };
        }

        // If not done already, gen and score noisy moves
        mNoisiesData.genAndScoreMoves<MoveGenType::NoisyOnly>(
            pos, mTtMove, MOVE_NONE, !mNoisiesOnly, historyTable
        );

        // Find next noisy move
        while (!mBadNoisyReady)
        {
            const auto [move, score] = mNoisiesData.next();

            if (move == MOVE_NONE) break;

            if (score < 0) {
                mBadNoisyReady = true;
                break;
            }

            // Yield good noisy move
            if (isPseudolegalLegal(pos, move))
                return { move, MoveRanking::GoodNoisy };
        }

        if (mNoisiesOnly) goto badNoisyMove;

        // Maybe return killer move
        if (!mKillerDone)
        {
            mKillerDone = true;

            if (mKiller != MOVE_NONE
            && pos.isQuiet(mKiller)
            && isPseudolegal(pos, mKiller)
            && isPseudolegalLegal(pos, mKiller))
                return { mKiller, MoveRanking::Killer };
        }

        // If not done already, gen and score quiet moves
        mQuietsData.genAndScoreMoves<MoveGenType::QuietOnly>(
            pos, mTtMove, mKiller, true, historyTable
        );

        // Yield quiet moves
        while (true)
        {
            const auto [move, score] = mQuietsData.next();

            if (move == MOVE_NONE) break;

            if (isPseudolegalLegal(pos, move))
                return { move, MoveRanking::Quiet };
        }

        badNoisyMove:

        // Yield bad noisy move
        if (mBadNoisyReady)
        {
            mBadNoisyReady = false;

            const Move move = mNoisiesData.mMoves[*(mNoisiesData.mIdx) - 1];

            if (isPseudolegalLegal(pos, move))
                return { move, MoveRanking::BadNoisy };

            return nextLegal(pos, historyTable);
        }

        // No more legal moves
        return { MOVE_NONE, std::nullopt };
    }

}; // struct MovePicker
