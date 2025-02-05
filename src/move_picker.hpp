// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "position.hpp"
#include "move_gen.hpp"

constexpr Move partialSelectionSort(
    ArrayVec<Move, 256>& moves, std::array<i32, 256>& movesScores, const size_t idx)
{
    assert(idx < moves.size());

    for (size_t i = idx + 1; i < moves.size(); i++)
        if (movesScores[i] > movesScores[idx])
        {
            moves.swap(i, idx);
            std::swap(movesScores[i], movesScores[idx]);
        }

    return moves[idx];
}

struct MovePicker
{
public:

    ArrayVec<Move, 256> mNoisies, mQuiets;

    std::array<i32, 256> mNoisiesScores, mQuietsScores;

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
        {
            mNoisies = pseudolegalMoves(pos, MoveGenType::NoisyOnly);

            for (size_t i = 0; i < mNoisies.size(); i++)
            {
                const Move move = mNoisies[i];
                const std::optional<PieceType> promo = move.promotion();

                mNoisiesScores[i] = promo && *promo == PieceType::Queen ? 10'000
                                  : promo ? -10'000
                                  : 0;

                // MVVLVA

                const std::optional<PieceType> captured = pos.captured(move);
                const i32 iCaptured = captured ? static_cast<i32>(*captured) + 1 : 0;

                const PieceType pt = move.pieceType();
                const i32 iPieceType = pt == PieceType::King ? 0 : static_cast<i32>(pt) + 1;

                mNoisiesScores[i] += iCaptured * 100 - iPieceType;
            }
        }

        while (static_cast<size_t>(++mNoisiesIdx) < mNoisies.size())
        {
            const Move move = partialSelectionSort(
                mNoisies, mNoisiesScores, static_cast<size_t>(mNoisiesIdx)
            );

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
