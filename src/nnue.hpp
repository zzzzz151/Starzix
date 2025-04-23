// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "simd.hpp"

// incbin fuckery
#ifdef _MSC_VER
    #define STARZIX_MSVC
    #pragma push_macro("_MSC_VER")
    #undef _MSC_VER
#endif

#include "../3rd-party/incbin.h"

namespace nnue {

constexpr size_t HIDDEN_LAYER_SIZE = 1024;
constexpr size_t NUM_INPUT_BUCKETS = 5;
constexpr i32 SCALE = 400;
constexpr i32 QA = 256;
constexpr i32 QB = 64;

// Enemy queen input buckets (0 if enemy doesnt have exactly 1 queen)
constexpr EnumArray<size_t, Square> INPUT_BUCKETS_MAP = {
//  A  B  C  D  E  F  G  H
    1, 1, 1, 1, 2, 2, 2, 2, // 1
    1, 1, 1, 1, 2, 2, 2, 2, // 2
    1, 1, 1, 1, 2, 2, 2, 2, // 3
    1, 1, 1, 1, 2, 2, 2, 2, // 4
    3, 3, 3, 3, 4, 4, 4, 4, // 5
    3, 3, 3, 3, 4, 4, 4, 4, // 6
    3, 3, 3, 3, 4, 4, 4, 4, // 7
    3, 3, 3, 3, 4, 4, 4, 4  // 8
//  A  B  C  D  E  F  G  H
};

// [hiddenNeuronIdx]
using HLArray = std::array<i16, HIDDEN_LAYER_SIZE>;

struct Net
{
public:

    using FtWeightsForBucket = EnumArray<HLArray, Color, PieceType, Square>;
    using FtWeightsForColor = std::array<FtWeightsForBucket, NUM_INPUT_BUCKETS>;

    // [perspectiveColor][inputBucket][pieceColor][pieceType][square][hiddenNeuronIdx]
    alignas(sizeof(Vec)) EnumArray<FtWeightsForColor, Color> ftWeights;

    // [perspectiveColor][hiddenNeuronIdx]
    alignas(sizeof(Vec)) EnumArray<HLArray, Color> hiddenBiases;

    // [isNotSideToMove][hiddenNeuronIdx]
    alignas(sizeof(Vec)) std::array<HLArray, 2> outputWeights;

    i32 outputBias;
};

INCBIN(NetFile, "src/net.bin");
const Net* NET = reinterpret_cast<const Net*>(gNetFileData);

struct FinnyTableEntry
{
public:

    alignas(sizeof(Vec)) HLArray accumulator;

    EnumArray<Bitboard, Color>     colorBbs;
    EnumArray<Bitboard, PieceType> piecesBbs;
};

// [mirrorVAxis][inputBucket]
using FinnyTableForColor = MultiArray<FinnyTableEntry, 2, NUM_INPUT_BUCKETS>;

// [accumulatorColor][mirrorVAxis][inputBucket]
using FinnyTable = EnumArray<FinnyTableForColor, Color>;

struct BothAccumulators
{
public:

    alignas(sizeof(Vec)) EnumArray<HLArray, Color> mAccumulators;

    // If a king is on right side of board,
    // mirror all pieces along vertical axis
    // in that color's accumulator
    EnumArray<bool, Color> mMirrorVAxis = { false, false };

    EnumArray<size_t, Color> mInputBucket = { 0, 0 };

    bool mUpdated = false;

    constexpr bool operator==(const BothAccumulators other) const
    {
        return mAccumulators == other.mAccumulators
            && mMirrorVAxis == other.mMirrorVAxis
            && mInputBucket == other.mInputBucket
            && mUpdated == other.mUpdated;
    }

    constexpr BothAccumulators() { } // Does not init mAccumulators

    constexpr BothAccumulators(const Position& pos)
    {
        // Init mMirrorVAxis
        for (const Color color : EnumIter<Color>())
        {
            const File kingFile = squareFile(pos.kingSquare(color));
            mMirrorVAxis[color] = static_cast<i32>(kingFile) >= static_cast<i32>(File::E);
        }

        // Init mInputBucket
        setInputBucket(Color::White, pos.getBb(Color::Black, PieceType::Queen));
        setInputBucket(Color::Black, pos.getBb(Color::White, PieceType::Queen));

        // Init mAccumulators

        mAccumulators = NET->hiddenBiases;

        const auto activateFeature = [&] (
            const Color pieceColor, const PieceType pt, const Square square) constexpr
        {
            for (const Color color : EnumIter<Color>())
            {
                const size_t inputBucket = mInputBucket[color];
                const Square newSquare = mMirrorVAxis[color] ? flipFile(square) : square;

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    mAccumulators[color][i]
                        += NET->ftWeights[color][inputBucket][pieceColor][pt][newSquare][i];
                }
            }
        };

        for (const Color pieceColor : EnumIter<Color>())
            for (const PieceType pt : EnumIter<PieceType>())
            {
                ITERATE_BITBOARD(pos.getBb(pieceColor, pt), square,
                {
                    activateFeature(pieceColor, pt, square);
                });
            }

        // Everything initialized
        mUpdated = true;
    }

private:

    constexpr size_t setInputBucket(const Color color, const Bitboard enemyQueensBb)
    {
        if (std::popcount(enemyQueensBb) != 1)
            mInputBucket[color] = 0;
        else {
            Square enemyQueenSquare = lsb(enemyQueensBb);

            if (mMirrorVAxis[color])
                enemyQueenSquare = flipFile(enemyQueenSquare);

            mInputBucket[color] = INPUT_BUCKETS_MAP[enemyQueenSquare];
        }

        return mInputBucket[color];
    }

    constexpr void updateFinnyEntryAndAccumulator(
        FinnyTable& finnyTable, const Color accColor, const Position& pos)
    {
        const size_t inputBucket = mInputBucket[accColor];
        const bool mirrorVAxis = mMirrorVAxis[accColor];

        FinnyTableEntry& finnyEntry = finnyTable[accColor][mirrorVAxis][inputBucket];

        const auto updatePieces = [&] (
            const Color pieceColor, const PieceType pt) constexpr
        {
            const Bitboard bb = pos.getBb(pieceColor, pt);
            const Bitboard entryBb = finnyEntry.colorBbs[pieceColor] & finnyEntry.piecesBbs[pt];

            const Bitboard toRemove = entryBb & ~bb;
            const Bitboard toAdd    = bb & ~entryBb;

            // Remove diff
            ITERATE_BITBOARD(toRemove, square,
            {
                const Square newSquare = mirrorVAxis ? flipFile(square) : square;

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    finnyEntry.accumulator[i]
                        -= NET->ftWeights[accColor][inputBucket][pieceColor][pt][newSquare][i];
                }
            });

            // Add diff
            ITERATE_BITBOARD(toAdd, square,
            {
                const Square newSquare = mirrorVAxis ? flipFile(square) : square;

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    finnyEntry.accumulator[i]
                        += NET->ftWeights[accColor][inputBucket][pieceColor][pt][newSquare][i];
                }
            });
        };

        for (const Color pieceColor : EnumIter<Color>())
            for (const PieceType pt : EnumIter<PieceType>())
                updatePieces(pieceColor, pt);

        mAccumulators[accColor] = finnyEntry.accumulator;

        finnyEntry.colorBbs  = pos.colorBbs();
        finnyEntry.piecesBbs = pos.piecesBbs();
    }

public:

    constexpr void updateMove(
        const BothAccumulators& prevBothAccs, const Position& pos, FinnyTable& finnyTable)
    {
        assert(prevBothAccs.mUpdated);

        if (mUpdated) {
            assert(*this == nnue::BothAccumulators(pos));
            return;
        }

        mMirrorVAxis = prevBothAccs.mMirrorVAxis;
        mInputBucket = prevBothAccs.mInputBucket;

        // Move has already been made
        const Color colorMoving = !pos.sideToMove();

        const Move move = pos.lastMove();
        assert(move);

        const Square from = move.from();
        const Square to   = move.to();

        const PieceType pieceType = move.pieceType();
        const std::optional<PieceType> promotion = move.promotion();
        const PieceType ptPlaced = promotion.value_or(pieceType);

        // If king moved, we update mMirrorVAxis[colorMoving]
        // Our vertical axis mirroring toggles if our king just crossed vertical axis
        if (pieceType == PieceType::King)
        {
            mMirrorVAxis[colorMoving]
                = static_cast<i32>(squareFile(to)) >= static_cast<i32>(File::E);
        }

        // Our input bucket may change if
        // our vertical axis mirroring toggled
        // or if we captured a queen
        if (mMirrorVAxis[colorMoving] != prevBothAccs.mMirrorVAxis[colorMoving]
        || pos.captured() == PieceType::Queen)
            setInputBucket(colorMoving, pos.getBb(!colorMoving, PieceType::Queen));

        // If our vertical axis mirroring toggled or our input bucket changed,
        // reset our accumulator
        if  (mMirrorVAxis[colorMoving] != prevBothAccs.mMirrorVAxis[colorMoving]
        || mInputBucket[colorMoving] != prevBothAccs.mInputBucket[colorMoving])
            updateFinnyEntryAndAccumulator(finnyTable, colorMoving, pos);

        // If our queen moved or we promoted to a queen, enemy's input bucket may have changed
        if (pieceType == PieceType::Queen || promotion == PieceType::Queen)
        {
            setInputBucket(!colorMoving, pos.getBb(colorMoving, PieceType::Queen));

            // If their input bucket changed, reset their accumulator
            if  (mInputBucket[!colorMoving] != prevBothAccs.mInputBucket[!colorMoving])
                updateFinnyEntryAndAccumulator(finnyTable, !colorMoving, pos);
        }

        // Update accumulators with the move played

        for (const Color color : EnumIter<Color>())
        {
            const size_t inputBucket = mInputBucket[color];

            // No need to update this color's accumulator if we just reset it
            if (mMirrorVAxis[color] != prevBothAccs.mMirrorVAxis[color]
            ||  inputBucket != prevBothAccs.mInputBucket[color])
                continue;

            const Square newFrom = mMirrorVAxis[color] ? flipFile(from) : from;
            const Square newTo   = mMirrorVAxis[color] ? flipFile(to)   : to;

            if (pos.captured().has_value())
            {
                const PieceType captured = *(pos.captured());

                Square capturedSq = move.flag() == MoveFlag::EnPassant
                                  ? enPassantRelative(to) : to;

                if (mMirrorVAxis[color])
                    capturedSq = flipFile(capturedSq);

                const auto& ftWeights = NET->ftWeights[color][inputBucket];

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    mAccumulators[color][i]
                        = prevBothAccs.mAccumulators[color][i]
                        - ftWeights[colorMoving][pieceType][newFrom][i]
                        + ftWeights[colorMoving][ptPlaced][newTo][i]
                        - ftWeights[!colorMoving][captured][capturedSq][i];
                }
            }
            else if (move.flag() == MoveFlag::Castling)
            {
                auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];

                if (mMirrorVAxis[color]) {
                    rookFrom = flipFile(rookFrom);
                    rookTo   = flipFile(rookTo);
                }

                const auto& ftWeights = NET->ftWeights[color][inputBucket][colorMoving];

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    mAccumulators[color][i]
                        = prevBothAccs.mAccumulators[color][i]
                        - ftWeights[pieceType][newFrom][i]
                        + ftWeights[ptPlaced][newTo][i]
                        - ftWeights[PieceType::Rook][rookFrom][i]
                        + ftWeights[PieceType::Rook][rookTo][i];
                }
            }
            else {
                const auto& ftWeights = NET->ftWeights[color][inputBucket][colorMoving];

                for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
                {
                    mAccumulators[color][i]
                        = prevBothAccs.mAccumulators[color][i]
                        - ftWeights[pieceType][newFrom][i]
                        + ftWeights[ptPlaced][newTo][i];
                }
            }
        }

        mUpdated = true; // Everything updated

        assert(*this == nnue::BothAccumulators(pos));
    }

}; // struct BothAccumulators

constexpr i32 evaluate(const BothAccumulators& bothAccs, const Color stm)
{
    assert(bothAccs.mUpdated);

    i32 sum = 0;

    // Activation function:
    // SCReLU(hiddenNeuron) = clamp(hiddenNeuron, 0, QA)^2

    #if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
        // N is 16 and 32 for avx2 and avx512, respectively

        const Vec vecZero = setEpi16(0);  // N i16 zeros
        const Vec vecQA   = setEpi16(QA); // N i16 QA's
        Vec vecSum = vecZero; // N/2 i32 zeros, the total running sum

        for (const Color color : { stm, !stm })
            for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i += sizeof(Vec) / sizeof(i16))
            {
                // Load the next N hidden neurons and clamp them to [0, QA]
                const i16& accStart = bothAccs.mAccumulators[color][i];
                Vec hiddenNeurons = loadVec(reinterpret_cast<const Vec*>(&accStart));
                hiddenNeurons = clampVec(hiddenNeurons, vecZero, vecQA);

                // Load the respective N output weights

                const i16& outputWeightsStart = NET->outputWeights[color != stm][i];

                const Vec outputWeights = loadVec(
                    reinterpret_cast<const Vec*>(&outputWeightsStart)
                );

                // Multiply each hidden neuron with its respective output weight
                // We use mullo, which multiplies in the i32 world but returns the results as i16's
                // since we know the results fit in an i16
                Vec result = mulloEpi16(hiddenNeurons, outputWeights);

                // Multiply with hidden neurons again (square part of SCReLU activation)
                // We use madd, which multiplies in the i32 world and adds adjacent pairs
                // 'result' becomes N/2 i32's
                result = maddEpi16(result, hiddenNeurons);

                vecSum = addEpi32(vecSum, result); // Add 'result' to 'vecSum'
            }

        sum = sumVec(vecSum); // Add the N/2 i32's to get final sum (i32)
    #else
        for (const Color color : { stm, !stm })
            for (size_t i = 0; i < HIDDEN_LAYER_SIZE; i++)
            {
                const i16 clipped = std::clamp<i16>(bothAccs.mAccumulators[color][i], 0, QA);
                const i16 x = clipped * NET->outputWeights[color != stm][i];
                sum += static_cast<i32>(x) * static_cast<i32>(clipped);
            }
    #endif

    return (sum / QA + NET->outputBias) * SCALE / (QA * QB);
}

} // namespace nnue
