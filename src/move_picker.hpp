// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"

struct MovePicker
{
public:

    constexpr static size_t NO_IDX = 255;

    ArrayVec<Move, 256> mNoisies, mQuiets;

    i32 mNoisiesIdx = -1, mQuietsIdx = -1;

    Bitboard mEnemyAttacks = 0;

    constexpr MovePicker(const Position& pos)
    {
        mEnemyAttacks = pos.attacks(pos.notSideToMove()) ^ squareBb(pos.kingSquare());

        assert([&] () {
            const auto allMoves = pseudolegalMoves(pos, MoveGenType::AllMoves,  true, mEnemyAttacks);
            const auto noisies  = pseudolegalMoves(pos, MoveGenType::NoisyOnly, true, mEnemyAttacks);
            const auto quiets   = pseudolegalMoves(pos, MoveGenType::QuietOnly, true, mEnemyAttacks);

            assert(noisies.size() + quiets.size() == allMoves.size());

            for (const Move move : noisies)
            {
                assert(!pos.isQuiet(move));
                assert(allMoves.contains(move));
            }

            for (const Move move : quiets)
            {
                assert(pos.isQuiet(move));
                assert(allMoves.contains(move));
            }

            return true;
        }());
    }

    constexpr Move nextLegal(Position& pos)
    {
        if (mNoisiesIdx == -1)
            mNoisies = pseudolegalMoves(pos, MoveGenType::NoisyOnly, true, mEnemyAttacks);

        while (static_cast<size_t>(++mNoisiesIdx) < mNoisies.size())
        {
            const Move move = mNoisies[static_cast<size_t>(mNoisiesIdx)];

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        if (mQuietsIdx == -1)
            mQuiets = pseudolegalMoves(pos, MoveGenType::QuietOnly, true, mEnemyAttacks);

        while (static_cast<size_t>(++mQuietsIdx) < mQuiets.size())
        {
            const Move move = mQuiets[static_cast<size_t>(mQuietsIdx)];

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        return MOVE_NONE;
    }

}; // struct MovePicker
