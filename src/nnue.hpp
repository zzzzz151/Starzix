// clang-format off

#pragma once

#ifdef _MSC_VER
#define STARZIX_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "3rdparty/incbin.h"

#include "board.hpp"
#include "search_params.hpp"
#include "simd.hpp"

namespace nnue {

constexpr std::array<int, 64> INPUT_BUCKETS = {
//  A  B  C  D  E  F  G  H
    1, 1, 1, 1, 2, 2, 2, 2, // 0
    1, 1, 1, 1, 2, 2, 2, 2, // 1
    1, 1, 1, 1, 2, 2, 2, 2, // 2
    1, 1, 1, 1, 2, 2, 2, 2, // 3
    3, 3, 3, 3, 4, 4, 4, 4, // 4
    3, 3, 3, 3, 4, 4, 4, 4, // 5
    3, 3, 3, 3, 4, 4, 4, 4, // 6 
    3, 3, 3, 3, 4, 4, 4, 4  // 7
};

constexpr i32 HIDDEN_LAYER_SIZE = 1024,
              NUM_INPUT_BUCKETS = 5,
              SCALE = 400, 
              QA = 255, 
              QB = 64;

struct Net {
    public:

    // [color][inputBucket][feature][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, NUM_INPUT_BUCKETS, 768, HIDDEN_LAYER_SIZE> featuresWeights;

    // [color][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> hiddenBiases;

    // [hiddenNeuronIdx]
    alignas(sizeof(Vec)) std::array<i16, HIDDEN_LAYER_SIZE> outputWeightsStm;
    alignas(sizeof(Vec)) std::array<i16, HIDDEN_LAYER_SIZE> outputWeightsNstm;

    i16 outputBias;
};

INCBIN(NetFile, "../nn-trainer/checkpoints/net768x2-queen-buckets-600.bin");
const Net* NET = (const Net*)gNetFileData;

struct Accumulator {
    public:

    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> mAccumulators;

    // HM (Horizontal mirroring)
    // If a king is on right side of board,
    // mirror all pieces horizontally (along vertical axis) 
    // in that color's accumulator
    std::array<bool, 2> mMirrorHorizontally = {false, false}; // [color]

    inline Accumulator() = default;

    inline Accumulator(Board &board) 
    {
        for (Color color : {Color::WHITE, Color::BLACK}) 
        {
            File kingFile = squareFile(board.kingSquare(color));
            mMirrorHorizontally[(int)color] = (int)kingFile >= (int)File::E;
        }

        resetAccumulator(Color::WHITE, board);
        resetAccumulator(Color::BLACK, board);
    }

    inline void resetAccumulator(const Color color, Board &board) 
    {
        mAccumulators[(int)color] = NET->hiddenBiases[(int)color];

        const int inputBucket = [&]
        {
            u64 enemyQueensBb = board.getBb(oppColor(color), PieceType::QUEEN);

            if (std::popcount(enemyQueensBb) != 1)
                return 0;

            Square enemyQueenSquare = lsb(enemyQueensBb);

            if (mMirrorHorizontally[(int)color])
                enemyQueenSquare ^= 7;

            return INPUT_BUCKETS[enemyQueenSquare];
        }();

        for (Color pieceColor : {Color::WHITE, Color::BLACK})
            for (int pt = PAWN; pt <= KING; pt++)
            {
                u64 bb = board.getBb(pieceColor, (PieceType)pt);

                while (bb > 0) {
                    int square = poplsb(bb);

                    if (mMirrorHorizontally[(int)color])
                        square ^= 7;

                    const int ft = (int)pieceColor * 384 + pt * 64 + (int)square;

                    for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                        mAccumulators[(int)color][i] += NET->featuresWeights[(int)color][inputBucket][ft][i];
                }
            }
    }

}; // struct Accumulator

inline i32 evaluate(Accumulator* accumulator, Board &board, bool materialScale)
{
    int stm = (int)board.sideToMove();
    i32 sum = 0;

    // Activation function:
    // SCReLU(hiddenNeuron) = clamp(hiddenNeuron, 0, QA)^2

    #if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
        // N is 32 if avx512 else 16 if avx2

        const Vec vecZero = setEpi16(0);  // N i16 zeros
        const Vec vecQA   = setEpi16(QA); // N i16 QA's
        Vec vecSum = vecZero; // N/2 i32 zeros, the total running sum

        // Takes N i16 hidden neurons, along with N i16 output weights
        // Activates the hidden neurons (SCReLU) and multiplies each with its output weight
        // End result is N/2 i32's, which are summed to 'vecSum'
        auto sumSlice = [&](const i16 &accumulatorStart, const i16 &outputWeightsStart) -> void
        {
            // Load N hidden neurons and clamp them to [0, QA]
            Vec hiddenNeurons = loadVec((Vec*)&accumulatorStart);
            hiddenNeurons = clampVec(hiddenNeurons, vecZero, vecQA);

            // Load the respective N output weights
            Vec outputWeights = loadVec((Vec*)&outputWeightsStart);

            // Multiply each hidden neuron with its respective output weight
            // We use mullo, which multiplies in the i32 world but returns the results as i16's
            // since we know the results fit in an i16
            Vec result = mulloEpi16(hiddenNeurons, outputWeights);

            // Multiply with hidden neurons again (square part of SCReLU)
            // We use madd, which multiplies in the i32 world and adds adjacent pairs
            // 'result' becomes N/2 i32's
            result = maddEpi16(result, hiddenNeurons);

            vecSum = addEpi32(vecSum, result); // Add 'result' to 'vecSum'
        };

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i += sizeof(Vec) / sizeof(i16))
        {
            // From each accumulator, take the next N i16 hidden neurons 
            // and their i16 output weights (1 per hidden neuron, so N)
            sumSlice(accumulator->mAccumulators[stm][i], NET->outputWeightsStm[i]);
            sumSlice(accumulator->mAccumulators[!stm][i], NET->outputWeightsNstm[i]);
        }

        sum = sumVec(vecSum); // Add the N/2 i32's to get final sum (i32)
    #else
        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            i16 clipped = std::clamp<i16>(accumulator->mAccumulators[stm][i], 0, QA);
            i16 x = clipped * NET->outputWeightsStm[i];
            sum += x * clipped;
        }

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            i16 clipped = std::clamp<i16>(accumulator->mAccumulators[!stm][i], 0, QA);
            i16 x = clipped * NET->outputWeightsNstm[i];
            sum += x * clipped;
        }
    #endif

    i32 eval = (sum / QA + NET->outputBias) * SCALE / (QA * QB);

    // Scale eval with material / game phase
    if (materialScale) {
        i32 material = 3 * std::popcount(board.getBb(PieceType::KNIGHT))
                     + 3 * std::popcount(board.getBb(PieceType::BISHOP))
                     + 5 * std::popcount(board.getBb(PieceType::ROOK))
                     + 9 * std::popcount(board.getBb(PieceType::QUEEN));

        constexpr i32 MATERIAL_MAX = 62;

        // Linear lerp from evalMaterialScaleMin to evalMaterialScaleMax as material goes from 0 to MATERIAL_MAX
        eval *= evalMaterialScaleMin() + (evalMaterialScaleMax() - evalMaterialScaleMin()) * material / MATERIAL_MAX;
    }

    return eval;
}

} // namespace nnue

using Accumulator = nnue::Accumulator;
