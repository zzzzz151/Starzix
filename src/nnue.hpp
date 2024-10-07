// clang-format off

#pragma once
/*
// incbin fuckery
#ifdef _MSC_VER
    #define STARZIX_MSVC
    #pragma push_macro("_MSC_VER")
    #undef _MSC_VER
#endif
*/

#include "utils.hpp"
//#include "board.hpp"
//#include "simd.hpp"
//#include "search_params.hpp"
//#include "3rdparty/incbin.h"
//#include <algorithm>

/*

namespace nnue {

constexpr i32 HIDDEN_LAYER_SIZE = 1024,
              NUM_INPUT_BUCKETS = 5,
              SCALE = 400, 
              QA = 255, 
              QB = 64;

// Enemy queen input buckets (0 if enemy doesnt have exactly 1 queen)
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

    // [color][inputBucket][feature768][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, NUM_INPUT_BUCKETS, 768, HIDDEN_LAYER_SIZE> featuresWeights;

    // [color][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> hiddenBiases;

    // [0 = stm | 1 = nstm][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> outputWeights;

    i16 outputBias;
};

INCBIN(NetFile, "src/net.bin");
const Net* NET = (const Net*)gNetFileData;

struct FinnyTableEntry {
    public:

    // [hiddenNeuronIdx]
    alignas(sizeof(Vec)) std::array<i16, HIDDEN_LAYER_SIZE> accumulator;

    std::array<u64, 2> colorBitboards;  // [color]
    std::array<u64, 6> piecesBitboards; // [pieceType]
};

// [color][mirrorHorizontally][inputBucket]
using FinnyTable = MultiArray<FinnyTableEntry, 2, 2, NUM_INPUT_BUCKETS>; 

struct BothAccumulators {
    public:

    // [color][hiddenNeuronIdx]
    alignas(sizeof(Vec)) MultiArray<i16, 2, HIDDEN_LAYER_SIZE> mAccumulators;

    // HM (Horizontal mirroring)
    // If a king is on right side of board,
    // mirror all pieces horizontally (along vertical axis) 
    // in that color's accumulator
    std::array<bool, 2> mMirrorHorizontally = {false, false}; // [color]

    std::array<int, 2> mInputBucket = {0, 0}; // [color]

    bool mUpdated = false;

    constexpr bool operator==(const BothAccumulators other) const 
    {
        return mAccumulators == other.mAccumulators
               && mMirrorHorizontally == other.mMirrorHorizontally
               && mInputBucket == other.mInputBucket
               && mUpdated == other.mUpdated;
    }

    constexpr BothAccumulators() = default;

    inline BothAccumulators(const Board &board) 
    {
        mAccumulators = NET->hiddenBiases;

        for (const Color color : {Color::WHITE, Color::BLACK}) 
        {
            const File kingFile = squareFile(board.kingSquare(color));
            mMirrorHorizontally[(int)color] = (int)kingFile >= (int)File::E;
        }

        setInputBucket(Color::WHITE, board.getBb(Color::BLACK, PieceType::QUEEN));
        setInputBucket(Color::BLACK, board.getBb(Color::WHITE, PieceType::QUEEN));

        auto activatePiece = [&](Color pieceColor, PieceType pt) constexpr -> void
        {
            u64 bb = board.getBb(pieceColor, pt);

            while (bb > 0) {
                Square sq = poplsb(bb);

                for (const int color : {WHITE, BLACK}) 
                {
                    const auto inputBucket = mInputBucket[color];
                    const auto ft768 = feature768(pieceColor, pt, sq, mMirrorHorizontally[color]);

                     for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                        mAccumulators[color][i] += NET->featuresWeights[color][inputBucket][ft768][i];
                }
            }
        };

        for (const Color pieceColor : {Color::WHITE, Color::BLACK})
            for (int pieceType = PAWN; pieceType <= KING; pieceType++)
                activatePiece(pieceColor, (PieceType)pieceType);

        mUpdated = true;
    }

    private:

    constexpr int setInputBucket(const Color color, const u64 enemyQueensBb)
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

    constexpr int feature768(
        const Color pieceColor, const PieceType pt, Square sq, const bool mirrorHorizontally) const
    {
        if (mirrorHorizontally) sq ^= 7;

        assert((int)pieceColor * 384 + (int)pt * 64 + (int)sq < 768);

        return (int)pieceColor * 384 + (int)pt * 64 + (int)sq;
    }

    constexpr void updateFinnyEntryAndAccumulator(FinnyTable &finnyTable, const Color accColor, const Board &board) 
    {
        const int iAccColor = (int)accColor;
        const auto hm = mMirrorHorizontally[iAccColor];
        const auto inputBucket = mInputBucket[iAccColor];

        FinnyTableEntry &finnyEntry = finnyTable[iAccColor][hm][inputBucket];

        auto updatePiece = [&](Color pieceColor, PieceType pt) -> void
        {
            const u64 bb = board.getBb(pieceColor, pt);
            const u64 entryBb = finnyEntry.colorBitboards[(int)pieceColor] & finnyEntry.piecesBitboards[(int)pt];

            u64 add = bb & ~entryBb;
            u64 remove = entryBb & ~bb;

            while (remove > 0) {
                const Square sq = poplsb(remove);
                const auto ft768 = feature768(pieceColor, pt, sq, hm);

                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
                    finnyEntry.accumulator[i] -= NET->featuresWeights[iAccColor][inputBucket][ft768][i];
            }

            while (add > 0) {
                const Square sq = poplsb(add);
                const auto ft768 = feature768(pieceColor, pt, sq, hm);

                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
                    finnyEntry.accumulator[i] += NET->featuresWeights[iAccColor][inputBucket][ft768][i];
            }
        };

        for (const Color pieceColor : {Color::WHITE, Color::BLACK})
            for (int pieceType = PAWN; pieceType <= KING; pieceType++)
                updatePiece(pieceColor, (PieceType)pieceType);

        mAccumulators[iAccColor] = finnyEntry.accumulator;

        board.getColorBitboards(finnyEntry.colorBitboards);
        board.getPiecesBitboards(finnyEntry.piecesBitboards);
    }

    public:

    constexpr void update(BothAccumulators* prevBothAccs, const Board &board, FinnyTable &finnyTable)
    {
        assert(prevBothAccs->mUpdated && !mUpdated);

        mMirrorHorizontally = prevBothAccs->mMirrorHorizontally;
        mInputBucket = prevBothAccs->mInputBucket;

        const Color colorMoving = board.oppSide();
        const Color notColorMoving = oppColor(colorMoving);
        const int iColorMoving = (int)colorMoving;

        const Move move = board.lastMove();
        assert(move != MOVE_NONE);

        const int from = move.from();
        const int to = move.to();
        const PieceType pieceType = move.pieceType();
        const PieceType promotion = move.promotion();
        const PieceType place = promotion != PieceType::NONE ? promotion : pieceType;

        // If king moved, we update mMirrorHorizontally[colorMoving]
        // Our HM toggles if our king just crossed vertical axis
        if (pieceType == PieceType::KING) 
            mMirrorHorizontally[iColorMoving] = (int)squareFile(to) >= (int)File::E;

        // Our input bucket may change if our HM toggled or if we captured a queen
        if (mMirrorHorizontally[iColorMoving] != prevBothAccs->mMirrorHorizontally[iColorMoving] 
        || board.captured() == PieceType::QUEEN)
            setInputBucket(colorMoving, board.getBb(notColorMoving, PieceType::QUEEN));

        // If our HM toggled or our input bucket changed, reset our accumulator
        if  (mMirrorHorizontally[iColorMoving] != prevBothAccs->mMirrorHorizontally[iColorMoving]
        || mInputBucket[iColorMoving] != prevBothAccs->mInputBucket[iColorMoving])
            updateFinnyEntryAndAccumulator(finnyTable, colorMoving, board);

        // If our queen moved or we promoted to a queen, enemy's input bucket may have changed
        if (pieceType == PieceType::QUEEN || promotion == PieceType::QUEEN)
        {
            setInputBucket(notColorMoving, board.getBb(colorMoving, PieceType::QUEEN));

            // If their input bucket changed, reset their accumulator
            if  (mInputBucket[!iColorMoving] != prevBothAccs->mInputBucket[!iColorMoving])
                updateFinnyEntryAndAccumulator(finnyTable, notColorMoving, board);
        }

        // Update accumulators with the move played

        for (const int color : {WHITE, BLACK})
        {
            const bool hm = mMirrorHorizontally[color];
            const auto inputBucket = mInputBucket[color];

            // No need to update this color's accumulator if we just reset it
            if (hm != prevBothAccs->mMirrorHorizontally[color] || inputBucket != prevBothAccs->mInputBucket[color]) 
                continue;

            const auto subPieceFeature768 = feature768(colorMoving, pieceType, from, hm);
            const auto addPieceFeature768 = feature768(colorMoving, place, to, hm);

            if (board.captured() != PieceType::NONE)
            {
                const Square capturedPieceSq = move.flag() == Move::EN_PASSANT_FLAG ? to ^ 8 : to;
                const auto subCapturedFeature768 = feature768(notColorMoving, board.captured(), capturedPieceSq, hm);

                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        prevBothAccs->mAccumulators[color][i]
                        - NET->featuresWeights[color][inputBucket][subPieceFeature768][i]  
                        + NET->featuresWeights[color][inputBucket][addPieceFeature768][i]
                        - NET->featuresWeights[color][inputBucket][subCapturedFeature768][i];
            }
            else if (move.flag() == Move::CASTLING_FLAG)
            {
                const auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
                const auto subRookFeature768 = feature768(colorMoving, PieceType::ROOK, rookFrom, hm);
                const auto addRookFeature768 = feature768(colorMoving, PieceType::ROOK, rookTo, hm);

                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        prevBothAccs->mAccumulators[color][i] 
                        - NET->featuresWeights[color][inputBucket][subPieceFeature768][i]  
                        + NET->featuresWeights[color][inputBucket][addPieceFeature768][i]
                        - NET->featuresWeights[color][inputBucket][subRookFeature768][i]  
                        + NET->featuresWeights[color][inputBucket][addRookFeature768][i];
            }
            else {
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        prevBothAccs->mAccumulators[color][i]
                        - NET->featuresWeights[color][inputBucket][subPieceFeature768][i]  
                        + NET->featuresWeights[color][inputBucket][addPieceFeature768][i];
            }
        }

        mUpdated = true;
    }

}; // struct BothAccumulators

constexpr i32 evaluate(const BothAccumulators* bothAccs, const Color sideToMove)
{
    assert(bothAccs->mUpdated);

    const int stm = (int)sideToMove;
    i32 sum = 0;

    // Activation function:
    // SCReLU(hiddenNeuron) = clamp(hiddenNeuron, 0, QA)^2

    #if defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__))
        // N is 16 and 32 for avx2 and avx512, respectively

        const Vec vecZero = setEpi16(0);  // N i16 zeros
        const Vec vecQA   = setEpi16(QA); // N i16 QA's
        Vec vecSum = vecZero; // N/2 i32 zeros, the total running sum

        for (const int color : {stm, 1 - stm})
            for (int i = 0; i < HIDDEN_LAYER_SIZE; i += sizeof(Vec) / sizeof(i16))
            {
                // Load the next N hidden neurons and clamp them to [0, QA]
                const i16 &accStart = bothAccs->mAccumulators[color][i];
                Vec hiddenNeurons = loadVec((Vec*)&accStart);
                hiddenNeurons = clampVec(hiddenNeurons, vecZero, vecQA);

                // Load the respective N output weights
                const i16 &outputWeightsStart = NET->outputWeights[color != stm][i];
                const Vec outputWeights = loadVec((Vec*)&outputWeightsStart);

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
        for (const int color : {stm, 1 - stm})
            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            {
                const i16 clipped = std::clamp<i16>(bothAccs->mAccumulators[color][i], 0, QA);
                const i16 x = clipped * NET->outputWeights[color != stm][i];
                sum += x * clipped;
            }
    #endif

    return (sum / QA + NET->outputBias) * SCALE / (QA * QB);
}

} // namespace nnue

constexpr float materialScale(const Board &board) 
{
    const float material = 3 * std::popcount(board.getBb(PieceType::KNIGHT))
                         + 3 * std::popcount(board.getBb(PieceType::BISHOP))
                         + 5 * std::popcount(board.getBb(PieceType::ROOK))
                         + 9 * std::popcount(board.getBb(PieceType::QUEEN));

    // Linear lerp from evalMaterialScaleMin to evalMaterialScaleMax 
    // as material goes from 0 to MATERIAL_MAX
    return evalMaterialScaleMin() 
           + (evalMaterialScaleMax() - evalMaterialScaleMin()) 
           * material / (float)MATERIAL_MAX;
}

using BothAccumulators = nnue::BothAccumulators;
using FinnyTableEntry = nnue::FinnyTableEntry;
using FinnyTable = nnue::FinnyTable;
*/