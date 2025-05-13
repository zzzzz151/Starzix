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

enum class Operation : i32 {
    Add, Remove
};

constexpr size_t NUM_INPUT_BUCKETS = 6;
constexpr size_t HL_SIZE = 256;
constexpr i32 SCALE = 400;
constexpr i32 QA = 256;
constexpr i32 QB = 64;

// Enemy queen input buckets (0 if no enemy queens, NUM_INPUT_BUCKETS-1 if multiple)
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
using HLArray = std::array<i16, HL_SIZE>;

struct Net
{
public:

    using HLArrayPST = EnumArray<HLArray, Color, PieceType, Square>;

    // [inputBucket][pieceColor][pieceType][square][hiddenNeuronIdx]
    alignas(sizeof(Vec)) std::array<HLArrayPST, NUM_INPUT_BUCKETS> ftWeights;

    alignas(sizeof(Vec)) HLArray hiddenBiases;

    alignas(sizeof(Vec)) HLArray outputWeights;

    i32 outputBias;
};

INCBIN(NetFile, "src/net.bin");
const Net* NET = reinterpret_cast<const Net*>(gNetFileData);

struct Accumulator
{
public:

    alignas(sizeof(Vec)) HLArray mAcc;

    EnumArray<Bitboard, Color>     mColorBbs;
    EnumArray<Bitboard, PieceType> mPiecesBbs;

    bool mMirrorVAxis;

    size_t mInputBucket;

    bool mUpdated = false;

    // [stm][mirrorVAxis][inputBucket]
    using FinnyTable = EnumArray<MultiArray<Accumulator, 2, NUM_INPUT_BUCKETS>, Color>;

    constexpr bool operator==(const Accumulator& other) const
    {
        return mAcc == other.mAcc
            && mColorBbs == other.mColorBbs
            && mPiecesBbs == other.mPiecesBbs
            && mMirrorVAxis == other.mMirrorVAxis
            && mInputBucket == other.mInputBucket
            && mUpdated == other.mUpdated;
    }

    constexpr Accumulator() { } // Does not init fields not initialized by default

    inline Accumulator(const Position& pos)
    {
        setMetadata(pos);

        mAcc = NET->hiddenBiases;

        for (const Color pieceColor : EnumIter<Color>())
            for (const PieceType pt : EnumIter<PieceType>())
            {
                const Bitboard bb = pos.getBb(pieceColor, pt);
                updateFeatures<Operation::Add>(bb, pos.sideToMove(), pieceColor, pt);
            }

        mUpdated = true;
    }

private:

    // Sets mColorBbs, mPiecesBbs, mMirrorVAxis and mInputBucket
    constexpr void setMetadata(const Position& pos)
    {
        mColorBbs  = pos.colorBbs();
        mPiecesBbs = pos.piecesBbs();

        const File ourKingFile = squareFile(pos.kingSquare());
        mMirrorVAxis = static_cast<i32>(ourKingFile) >= static_cast<i32>(File::E);

        const Bitboard enemyQueensBb = pos.getBb(!pos.sideToMove(), PieceType::Queen);

        if (std::popcount(enemyQueensBb) == 0)
            mInputBucket = 0;
        else if (std::popcount(enemyQueensBb) > 1)
            mInputBucket = NUM_INPUT_BUCKETS - 1;
        else {
            const Square enemyQueenSq = oriented(lsb(enemyQueensBb), pos.sideToMove());
            mInputBucket = INPUT_BUCKETS_MAP[enemyQueenSq];
        }
    }

    constexpr Square oriented(Square square, const Color stm) const
    {
        if (stm == Color::Black)
            square = flipRank(square);

        if (mMirrorVAxis)
            square = flipFile(square);

        return square;
    }

    template<Operation op>
    constexpr void updateFeature(
        const Color stm, Color pieceColor, const PieceType pt, Square square)
    {
        if (stm == Color::Black)
            pieceColor = !pieceColor;

        square = oriented(square, stm);

        if constexpr (op == Operation::Add)
        {
            for (size_t i = 0; i < HL_SIZE; i++)
                mAcc[i] += NET->ftWeights[mInputBucket][pieceColor][pt][square][i];
        }
        else if constexpr (op == Operation::Remove)
        {
            for (size_t i = 0; i < HL_SIZE; i++)
                mAcc[i] -= NET->ftWeights[mInputBucket][pieceColor][pt][square][i];
        }
    }

    template<Operation op>
    constexpr void updateFeatures(
        const Bitboard bb, const Color stm, const Color pieceColor, const PieceType pt)
    {
        ITERATE_BITBOARD(bb, square,
        {
            updateFeature<op>(stm, pieceColor, pt, square);
        });
    }

public:

    constexpr void update(const Accumulator& prevAcc, const Position& pos, FinnyTable& finnyTable)
    {
        assert(prevAcc.mUpdated);

        if (mUpdated)
        {
            assert(*this == Accumulator(pos));
            return;
        }

        setMetadata(pos);

        Accumulator& finnyAcc = finnyTable[pos.sideToMove()][mMirrorVAxis][mInputBucket];

        const bool useFinnyTable
            = mMirrorVAxis != prevAcc.mMirrorVAxis || mInputBucket != prevAcc.mInputBucket;

        if (!useFinnyTable)
            mAcc = prevAcc.mAcc;

        for (const Color pieceColor : EnumIter<Color>())
            for (const PieceType pt : EnumIter<PieceType>())
            {
                const Bitboard bb = mColorBbs[pieceColor] & mPiecesBbs[pt];

                if (useFinnyTable)
                {
                    const Bitboard finnyBb
                        = finnyAcc.mColorBbs[pieceColor] & finnyAcc.mPiecesBbs[pt];

                    finnyAcc.updateFeatures<Operation::Remove>(
                        finnyBb & ~bb, pos.sideToMove(), pieceColor, pt
                    );

                    finnyAcc.updateFeatures<Operation::Add>(
                        bb & ~finnyBb, pos.sideToMove(), pieceColor, pt
                    );
                }
                else {
                    const Bitboard prevBb = prevAcc.mColorBbs[pieceColor] & prevAcc.mPiecesBbs[pt];

                    updateFeatures<Operation::Remove>(
                        prevBb & ~bb, pos.sideToMove(), pieceColor, pt
                    );

                    updateFeatures<Operation::Add>(
                        bb & ~prevBb, pos.sideToMove(), pieceColor, pt
                    );
                }
            }

        mUpdated = true;

        if (useFinnyTable)
        {
            finnyAcc.mColorBbs  = pos.colorBbs();
            finnyAcc.mPiecesBbs = pos.piecesBbs();

            mAcc = finnyAcc.mAcc;
        }

        assert(*this == Accumulator(pos));
    }

}; // struct Accumulator

constexpr i32 evaluate(const Accumulator& acc)
{
    assert(acc.mUpdated);

    i32 sum = 0;

    // Activation function:
    // SCReLU(hiddenNeuron) = clamp(hiddenNeuron, 0, QA)^2

    #if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
        // N is 16 and 32 for avx2 and avx512, respectively

        const Vec vecZero = setEpi16(0);  // N i16 zeros
        const Vec vecQA   = setEpi16(QA); // N i16 QA's
        Vec vecSum = vecZero; // N/2 i32 zeros, the total running sum

        for (size_t i = 0; i < HL_SIZE; i += sizeof(Vec) / sizeof(i16))
        {
            // Load the next N hidden neurons and clamp them to [0, QA]
            const i16& accStart = acc.mAcc[i];
            Vec hiddenNeurons = loadVec(reinterpret_cast<const Vec*>(&accStart));
            hiddenNeurons = clampVec(hiddenNeurons, vecZero, vecQA);

            // Load the respective N output weights

            const i16& outputWeightsStart = NET->outputWeights[i];

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
        std::cout << "Only avx2 and avx512 supported!" << std::endl;
        exit(1);
    #endif

    return (sum / QA + NET->outputBias) * SCALE / (QA * QB);
}

} // namespace nnue
