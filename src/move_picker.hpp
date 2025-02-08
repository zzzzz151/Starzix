// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"

struct MovesData
{
public:

    ArrayVec<Move, 256> mMoves;
    std::array<i32, 256> mScores;
    std::optional<size_t> mIdx = std::nullopt;

    template <MoveGenType moveGenType>
    constexpr void genAndScoreMoves(Position& pos, const Move ttMove)
    {
        static_assert(moveGenType != MoveGenType::AllMoves);

        if (mIdx) return;

        // Generate pseudolegal moves
        mMoves = pseudolegalMoves(pos, moveGenType);

        // Score moves
        for (size_t i = 0; i < mMoves.size();)
        {
            const Move move = mMoves[i];

            if (move == ttMove)
            {
                mMoves.swap(i, mMoves.size() - 1);
                mMoves.pop_back();
                continue;
            }

            if constexpr (moveGenType == MoveGenType::NoisyOnly)
            {
                const std::optional<PieceType> promo = move.promotion();

                constexpr EnumArray<i32, PieceType> PROMO_SCORE = {
                //  P     N        B        R        Q    K
                    0, -10'000, -30'000, -20'000, 10'000, 0
                };

                mScores[i] = promo ? PROMO_SCORE[*promo] : 5'000;

                // MVVLVA (most valuable victim, least valuable attacker)

                const std::optional<PieceType> captured = pos.captured(move);
                const i32 iCaptured = captured ? static_cast<i32>(*captured) + 1 : 0;

                const PieceType pt = move.pieceType();
                const i32 iPieceType = pt == PieceType::King ? 0 : static_cast<i32>(pt) + 1;

                mScores[i] += iCaptured * 100 - iPieceType;
            }
            else if constexpr (moveGenType == MoveGenType::QuietOnly)
            {
                mScores[i] = 0;
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

    bool mNoisiesOnly = false;
    Move mTtMove = MOVE_NONE;
    bool nextLegalCalled = false;
    MovesData mNoisiesData, mQuietsData;

public:

    constexpr MovePicker(const bool noisiesOnly, const Move ttMove)
    {
        mNoisiesOnly = noisiesOnly;
        mTtMove = ttMove;
    }

    constexpr Move nextLegal(Position& pos)
    {
        // Maybe return TT move the first time this function is called
        if (!nextLegalCalled)
        {
            nextLegalCalled = true;

            if (mTtMove != MOVE_NONE
            && (!mNoisiesOnly || !pos.isQuiet(mTtMove))
            && isPseudolegal(pos, mTtMove)
            && isPseudolegalLegal(pos, mTtMove))
                return mTtMove;
        }

        // If not done already, gen and score noisy moves
        mNoisiesData.genAndScoreMoves<MoveGenType::NoisyOnly>(pos, mTtMove);

        // Yield good noisy moves
        while (true) {
            const auto [move, score] = mNoisiesData.next();

            if (move == MOVE_NONE) break;

            if (score < 0) {
                *(mNoisiesData.mIdx) -= 1;
                break;
            }

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        if (mNoisiesOnly) goto badNoisyMoves;

        // If not done already, gen and score quiet moves
        mQuietsData.genAndScoreMoves<MoveGenType::QuietOnly>(pos, mTtMove);

        // Yield quiet moves
        while (true) {
            const auto [move, score] = mQuietsData.next();

            if (move == MOVE_NONE) break;

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        badNoisyMoves:

        // Yield bad noisy moves
        while (true) {
            const auto [move, score] = mNoisiesData.next();

            if (move == MOVE_NONE) break;

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        return MOVE_NONE;
    }

}; // struct MovePicker
