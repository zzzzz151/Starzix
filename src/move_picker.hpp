// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"

struct MovePicker
{
public:

    ArrayVec<Move, 256> mNoisies, mQuiets;

    i32 mNoisiesIdx = -1, mQuietsIdx = -1;

    constexpr Move nextLegal(Position& pos)
    {
        assert([&] () {
            if (mNoisiesIdx != -1) return true;

            const auto allMoves = pseudolegalMoves(pos, MoveGenType::AllMoves);
            const auto noisies  = pseudolegalMoves(pos, MoveGenType::NoisyOnly);
            const auto quiets   = pseudolegalMoves(pos, MoveGenType::QuietOnly);

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

        if (mNoisiesIdx == -1)
            mNoisies = pseudolegalMoves(pos, MoveGenType::NoisyOnly);

        while (static_cast<size_t>(++mNoisiesIdx) < mNoisies.size())
        {
            const Move move = mNoisies[static_cast<size_t>(mNoisiesIdx)];

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        if (mQuietsIdx == -1)
            mQuiets = pseudolegalMoves(pos, MoveGenType::QuietOnly);

        while (static_cast<size_t>(++mQuietsIdx) < mQuiets.size())
        {
            const Move move = mQuiets[static_cast<size_t>(mQuietsIdx)];

            if (isPseudolegalLegal(pos, move))
                return move;
        }

        return MOVE_NONE;
    }

}; // struct MovePicker
