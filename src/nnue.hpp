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

constexpr i32 HIDDEN_LAYER_SIZE = 1024,
              NUM_INPUT_BUCKETS = 5,
              SCALE = 400, 
              QA = 255, 
              QB = 64;

constexpr std::array<int, 64> INPUT_BUCKETS = {
//  A  B  C  D  E  F  G  H
    1, 1, 1, 1, 2, 2, 2, 2, // 1
    1, 1, 1, 1, 2, 2, 2, 2, // 2
    1, 1, 1, 1, 2, 2, 2, 2, // 3
    1, 1, 1, 1, 2, 2, 2, 2, // 4
    3, 3, 3, 3, 4, 4, 4, 4, // 5
    3, 3, 3, 3, 4, 4, 4, 4, // 6
    3, 3, 3, 3, 4, 4, 4, 4, // 7 
    3, 3, 3, 3, 4, 4, 4, 4  // 8
};

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

INCBIN(NetFile, "src/net.bin");
const Net* NET = (const Net*)gNetFileData;

struct Accumulator {
    public:

    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> mAccumulators;

    bool mUpdated = false;

    // HM (Horizontal mirroring)
    // If a king is on right side of board,
    // mirror all pieces horizontally (along vertical axis) 
    // in that color's accumulator
    std::array<bool, 2> mMirrorHorizontally = {false, false}; // [color]

    std::array<int, 2> mInputBucket = {0, 0}; // [color]

    inline Accumulator() = default;

    inline Accumulator(Board &board) 
    {
        for (Color color : {Color::WHITE, Color::BLACK}) 
        {
            File kingFile = squareFile(board.kingSquare(color));
            mMirrorHorizontally[(int)color] = (int)kingFile >= (int)File::E;
        }

        setInputBucket(Color::WHITE, board.getBb(Color::BLACK, PieceType::QUEEN));
        setInputBucket(Color::BLACK, board.getBb(Color::WHITE, PieceType::QUEEN));

        resetAccumulator(Color::WHITE, board);
        resetAccumulator(Color::BLACK, board);

        mUpdated = true;
    }

    inline int setInputBucket(Color color, u64 enemyQueensBb)
    {
        if (std::popcount(enemyQueensBb) != 1) 
            mInputBucket[(int)color] = 0;
        else {
            Square enemyQueenSquare = lsb(enemyQueensBb);

            if (mMirrorHorizontally[(int)color])
                enemyQueenSquare ^= 7;

            mInputBucket[(int)color] = INPUT_BUCKETS[enemyQueenSquare];
        }

        return mInputBucket[(int)color];
    }

    inline void resetAccumulator(Color color, Board &board) 
    {
        mAccumulators[(int)color] = NET->hiddenBiases[(int)color];
        const int inputBucket = mInputBucket[(int)color];
        
        for (Color pieceColor : {Color::WHITE, Color::BLACK})
            for (int pt = PAWN; pt <= KING; pt++)
            {
                u64 bb = board.getBb(pieceColor, (PieceType)pt);

                while (bb > 0) {
                    const int square = poplsb(bb);
                    const int ft = feature(color, pieceColor, (PieceType)pt, square);

                    for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                        mAccumulators[(int)color][i] += NET->featuresWeights[(int)color][inputBucket][ft][i];
                }
            }
    }

    inline int feature(Color color, Color pieceColor, PieceType pt, Square sq) const
    {
        if (mMirrorHorizontally[(int)color])
            sq ^= 7;

        return (int)pieceColor * 384 + (int)pt * 64 + (int)sq;
    }

    // [color]
    inline std::array<int, 2> features(Color pieceColor, PieceType pt, Square sq) const
    {
        return {
            feature(Color::WHITE, pieceColor, pt, sq),
            feature(Color::BLACK, pieceColor, pt, sq)
        };
    }

    inline void update(Accumulator* oldAcc, Board &board)
    {
        assert(oldAcc->mUpdated && !mUpdated);

        mMirrorHorizontally = oldAcc->mMirrorHorizontally;
        mInputBucket = oldAcc->mInputBucket;

        Move move = board.lastMove();
        assert(move != MOVE_NONE);

        const Color stm = board.oppSide(); // side that moved
        const Color nstm = oppColor(stm);
        const int from = move.from();
        const int to = move.to();
        const PieceType pieceType = move.pieceType();
        const PieceType promotion = move.promotion();
        const PieceType place = promotion != PieceType::NONE ? promotion : pieceType;

        // If stm king moved, we update mMirrorHorizontally[stm]
        if (pieceType == PieceType::KING) 
            mMirrorHorizontally[(int)stm] = (int)squareFile(to) >= (int)File::E;

        // If stm horizontal mirroring changed or stm captured a queen, we update mInputBucket[stm]
        if (mMirrorHorizontally[(int)stm] != oldAcc->mMirrorHorizontally[(int)stm] 
        || board.captured() == PieceType::QUEEN)
            setInputBucket(stm, board.getBb(nstm, PieceType::QUEEN));

        // If stm's horizontal mirroring or input bucket changed, reset stm accumulator
        if  (mMirrorHorizontally[(int)stm] != oldAcc->mMirrorHorizontally[(int)stm]
        || mInputBucket[(int)stm] != oldAcc->mInputBucket[(int)stm])
            resetAccumulator(stm, board);

        // If stm queen moved, or stm promoted to queen, update mInputBucket[nstm]
        if (pieceType == PieceType::QUEEN || promotion == PieceType::QUEEN)
        {
            setInputBucket(nstm, board.getBb(stm, PieceType::QUEEN));

            // If stm queen changed bucket, reset nstm accumulator
            if  (mInputBucket[(int)nstm] != oldAcc->mInputBucket[(int)nstm])
                resetAccumulator(nstm, board);
        }

        // [color]
        const std::array<int, 2> subPieceFeature = features(stm, pieceType, from);
        const std::array<int, 2> addPieceFeature = features(stm, place, to);
        std::array<int, 2> subCapturedFeature;

        if (board.captured() != PieceType::NONE)
        {
            int capturedPieceSq = move.flag() == Move::EN_PASSANT_FLAG 
                                  ? (to > from ? to - 8 : to + 8)
                                  : to;

            subCapturedFeature = features(oppColor(stm), board.captured(), capturedPieceSq);
        }

        for (int color : {WHITE, BLACK})
        {
            // No need to update this color's accumulator if we just resetted it
            // (either due to that king crossing the vertical axis or the color's input bucket changing)
            if (mMirrorHorizontally[color] != oldAcc->mMirrorHorizontally[color]
            || mInputBucket[color] != oldAcc->mInputBucket[color]) 
                continue;

            if (board.captured() != PieceType::NONE)
            {
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][mInputBucket[color]][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][mInputBucket[color]][addPieceFeature[color]][i]
                        - NET->featuresWeights[color][mInputBucket[color]][subCapturedFeature[color]][i];
            }
            else if (move.flag() == Move::CASTLING_FLAG)
            {
                auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];

                // [color]
                const std::array<int, 2> subRookFeature = features(stm, PieceType::ROOK, rookFrom);
                const std::array<int, 2> addRookFeature = features(stm, PieceType::ROOK, rookTo);

                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        oldAcc->mAccumulators[color][i] 
                        - NET->featuresWeights[color][mInputBucket[color]][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][mInputBucket[color]][addPieceFeature[color]][i]
                        - NET->featuresWeights[color][mInputBucket[color]][subRookFeature[color]][i]  
                        + NET->featuresWeights[color][mInputBucket[color]][addRookFeature[color]][i];
            }
            else {
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][mInputBucket[color]][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][mInputBucket[color]][addPieceFeature[color]][i];
            }
        }

        mUpdated = true;
    }

}; // struct Accumulator

inline i32 evaluate(Accumulator* accumulator, Board &board, bool materialScale)
{
    assert(accumulator->mUpdated);

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
