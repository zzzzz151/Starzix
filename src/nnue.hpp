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
using namespace SIMD;

namespace nnue {

constexpr int HIDDEN_LAYER_SIZE = 1024;
constexpr i32 SCALE = 400, QA = 255, QB = 64;
constexpr int WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(i16);

struct alignas(ALIGNMENT) Net {
    public:
    MultiArray<i16, 2, 768, HIDDEN_LAYER_SIZE> featuresWeights; // [color][feature][hiddenNeuron]
    MultiArray<i16, 2, HIDDEN_LAYER_SIZE>      hiddenBiases;    // [color][hiddenNeuron]
    MultiArray<i16, 2, HIDDEN_LAYER_SIZE>      outputWeights;   // [color][hiddenNeuron]
    i16                                        outputBias;
};

INCBIN(NetFile, "src/net.bin");
const Net* NET = (const Net*)(gNetFileData);

struct alignas(ALIGNMENT) Accumulator
{
    public:
    MultiArray<i16, 2, HIDDEN_LAYER_SIZE> mAccumulators = NET->hiddenBiases;
    bool mUpdated = false;

    inline Accumulator() = default;

    inline Accumulator(const Board &board) {
        mAccumulators = NET->hiddenBiases;

        for (Color color : {Color::WHITE, Color::BLACK})
            for (int pt = PAWN; pt <= KING; pt++)
                {
                    u64 bb = board.getBb(color, (PieceType)pt);
                    activateAll(bb, (color == Color::BLACK) * 384 + pt * 64);
                }

        mUpdated = true;
    }

    inline void activateAll(u64 bitboard, int feature) {
        while (bitboard > 0) {
            int square = poplsb(bitboard);

            for (int color : {WHITE, BLACK})
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] += NET->featuresWeights[color][feature + square][i];
        }
    }

    inline void update(Accumulator* oldAcc, Board &board)
    {
        assert(oldAcc->mUpdated && !mUpdated);

        Move move = board.lastMove();
        assert(move != MOVE_NONE);

        const auto moveFlag = move.flag();
        const int from = move.from();
        const int to = move.to();
        const PieceType pieceType = move.pieceType();
        const PieceType promotion = move.promotion();
        const PieceType place = promotion != PieceType::NONE ? promotion : pieceType;

        const int stm = (int)board.oppSide(); // side that moved

        const int subPieceFeature = stm * 384 + (int)pieceType * 64 + from;
        const int addPieceFeature = stm * 384 + (int)place * 64 + to;

        if (board.captured() != PieceType::NONE)
        {
            const int capturedPieceSq = moveFlag == Move::EN_PASSANT_FLAG 
                                        ? (to > from ? to - 8 : to + 8)
                                        : to;

            const int subCapturedFeature = !stm * 384 + (int)board.captured() * 64 + capturedPieceSq;

            for (int color : {WHITE, BLACK})
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][subPieceFeature][i]  
                        + NET->featuresWeights[color][addPieceFeature][i]
                        - NET->featuresWeights[color][subCapturedFeature][i];

        }
        else if (moveFlag == Move::CASTLING_FLAG)
        {
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            const int subRookFeature = stm * 384 + ROOK * 64 + (int)rookFrom;
            const int addRookFeature = stm * 384 + ROOK * 64 + (int)rookTo;

            for (int color : {WHITE, BLACK})
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = oldAcc->mAccumulators[color][i] 
                        - NET->featuresWeights[color][subPieceFeature][i]  
                        + NET->featuresWeights[color][addPieceFeature][i]
                        - NET->featuresWeights[color][subRookFeature][i]  
                        + NET->featuresWeights[color][addRookFeature][i]; 
         
        }
        else {
            for (int color : {WHITE, BLACK})
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][subPieceFeature][i]  
                        + NET->featuresWeights[color][addPieceFeature][i];     
        }

        mUpdated = true;
    }

}; // struct Accumulator

inline i32 evaluate(Accumulator* accumulator, Board &board, bool materialScale)
{
    assert(accumulator->mUpdated);

    int stm = (int)board.sideToMove();
    Vec *stmAccumulator = (Vec*) &(accumulator->mAccumulators[stm]);
    Vec *oppAccumulator = (Vec*) &(accumulator->mAccumulators[!stm]);

    Vec *stmWeights = (Vec*) &(NET->outputWeights[0]);
    Vec *oppWeights = (Vec*) &(NET->outputWeights[1]);
    const Vec vecZero = vecSetZero();
    const Vec vecQA = vecSet1Epi16(QA);
    Vec sum = vecSetZero();
    Vec v0, v1;

    for (int i = 0; i < HIDDEN_LAYER_SIZE / WEIGHTS_PER_VEC; ++i) {
        // Side to move
        v0 = maxEpi16(stmAccumulator[i], vecZero); // clip
        v0 = minEpi16(v0, vecQA); // clip
        v1 = mulloEpi16(v0, stmWeights[i]); // square
        v1 = maddEpi16(v1, v0); // multiply with output layer
        sum = addEpi32(sum, v1); // collect the result

        // Non side to move
        v0 = maxEpi16(oppAccumulator[i], vecZero);
        v0 = minEpi16(v0, vecQA);
        v1 = mulloEpi16(v0, oppWeights[i]);
        v1 = maddEpi16(v1, v0);
        sum = addEpi32(sum, v1);
    }

    i32 eval = (vecHaddEpi32(sum) / QA + NET->outputBias) * SCALE / (QA * QB);

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
