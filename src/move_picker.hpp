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

    constexpr Move next()
    {
        assert(mIdx);

        if (*mIdx >= mMoves.size())
            return MOVE_NONE;

        // Incremental selection sort
        for (size_t i = *mIdx + 1; i < mMoves.size(); i++)
            if (mScores[i] > mScores[*mIdx])
            {
                mMoves.swap(i, *mIdx);
                std::swap(mScores[i], mScores[*mIdx]);
            }

        return mMoves[(*mIdx)++];
    }

}; // struct MovesData

struct MovePicker
{
private:

    MovesData mNoisiesData, mQuietsData;

public:

    constexpr Move nextLegal(Position& pos)
    {
        Move move;

        // Noisies, no underpromos
        if ((move = nextLegalNoisy(pos, false)) != MOVE_NONE)
            return move;

        // Quiets
        if ((move = nextLegalQuiet(pos)) != MOVE_NONE)
            return move;

        // Underpromos
        if ((move = nextLegalNoisy(pos, true)) != MOVE_NONE)
            return move;

        return MOVE_NONE;
    }

    constexpr Move nextLegalNoisy(Position& pos, const bool underpromos)
    {
        // If noisy moves not generated, generate and score them
        if (!mNoisiesData.mIdx)
        {
            mNoisiesData.mMoves = pseudolegalMoves(pos, MoveGenType::NoisyOnly);

            // Score noisy moves
            for (size_t i = 0; i < mNoisiesData.mMoves.size(); i++)
            {
                const Move move = mNoisiesData.mMoves[i];
                const std::optional<PieceType> promo = move.promotion();

                constexpr EnumArray<i32, PieceType> promoBaseScore = {
                //  P     N        B        R        Q    K
                    0, -10'000, -30'000, -20'000, 10'000, 0
                };

                mNoisiesData.mScores[i] = promo ? promoBaseScore[*promo] : 0;

                // MVVLVA (most valuable victim, least valuable attacker)

                const std::optional<PieceType> captured = pos.captured(move);
                const i32 iCaptured = captured ? static_cast<i32>(*captured) + 1 : 0;

                const PieceType pt = move.pieceType();
                const i32 iPieceType = pt == PieceType::King ? 0 : static_cast<i32>(pt) + 1;

                mNoisiesData.mScores[i] += iCaptured * 100 - iPieceType;
            }

            mNoisiesData.mIdx = 0;
        }

        Move move;

        while ((move = mNoisiesData.next()) != MOVE_NONE)
        {
            if (!underpromos && move.isUnderpromotion())
            {
                *(mNoisiesData.mIdx) -= 1;
                return MOVE_NONE;
            }

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        return MOVE_NONE;
    }

    constexpr Move nextLegalQuiet(Position& pos)
    {
        // If quiet moves not generated, generate and score them
        if (!mQuietsData.mIdx)
        {
            mQuietsData.mMoves = pseudolegalMoves(pos, MoveGenType::QuietOnly);
            mQuietsData.mIdx = 0;
        }

        Move move;

        while (mQuietsData.mIdx < mQuietsData.mMoves.size())
        {
            move = mQuietsData.mMoves[(*(mQuietsData.mIdx))++];

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        /*
        while ((move = mQuietsData.next()) != MOVE_NONE)
            if (isPseudolegalLegal(pos, move))
                return move;
        */

        return MOVE_NONE;
    }

}; // struct MovePicker
