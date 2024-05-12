// clang-format off

#pragma once

#ifdef _MSC_VER
#define STARZIX_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "3rdparty/incbin.h"

#include "board.hpp"
#include "simd.hpp"
using namespace SIMD;

namespace nnue {

const int HIDDEN_LAYER_SIZE = 1024;
const i32 SCALE = 400, QA = 255, QB = 64;
constexpr int WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(i16);

struct alignas(ALIGNMENT) Net {
    std::array<std::array<i16, HIDDEN_LAYER_SIZE>, 768> featureWeights;
    std::array<i16, HIDDEN_LAYER_SIZE>                  featureBiases;
    std::array<std::array<i16, HIDDEN_LAYER_SIZE>, 2>   outputWeights;
    i16                                                 outputBias;
};

INCBIN(NetFile, "src/net.nnue");
const Net *NET = reinterpret_cast<const Net*>(gNetFileData);

struct alignas(ALIGNMENT) Accumulator
{
    public:
    std::array<i16, HIDDEN_LAYER_SIZE> white = NET->featureBiases;
    std::array<i16, HIDDEN_LAYER_SIZE> black = NET->featureBiases;
    bool updated = false;

    inline Accumulator() = default;

    inline Accumulator(const Board &board) {
        white = black = NET->featureBiases;

        for (Color color : {Color::WHITE, Color::BLACK})
            for (int pt = (int)PieceType::PAWN; pt <= (int)PieceType::KING; pt++)
                {
                    u64 bb = board.bitboard(color, (PieceType)pt);
                    while (bb > 0) {
                        Square sq = poplsb(bb);
                        activate(color, (PieceType)pt, sq);
                    }
                }

        updated = true;
    }

    inline void activate(Color color, PieceType pieceType, Square square)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + (int)square;
        int blackIdx = !int(color) * 384 + (int)pieceType * 64 + int(square ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) {
            white[i] += NET->featureWeights[whiteIdx][i];
            black[i] += NET->featureWeights[blackIdx][i];
        }
    }

    inline void deactivate(Color color, PieceType pieceType, Square square)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + (int)square;
        int blackIdx = !int(color) * 384 + (int)pieceType * 64 + int(square ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) {
            white[i] -= NET->featureWeights[whiteIdx][i];
            black[i] -= NET->featureWeights[blackIdx][i];
        }
    }

    inline void update(Accumulator &oldAcc, Color stm, Move move, PieceType captured)
    {
        assert(oldAcc.updated && !updated);
        assert(move != MOVE_NONE);

        auto moveFlag = move.flag();
        Square from = move.from();
        Square to = move.to();
        PieceType pieceType = move.pieceType();
        PieceType promotion = move.promotion();
        PieceType place = promotion != PieceType::NONE ? promotion : pieceType;

        // Remove piece from origin
        int sub1WhiteIdx = (int)stm * 384 + (int)pieceType * 64 + (int)from;
        int sub1BlackIdx = !int(stm)* 384 + (int)pieceType * 64 + int(from ^ 56);

        // Put piece on destination
        int add1WhiteIdx = (int)stm * 384 + (int)place * 64 + (int)to;
        int add1BlackIdx = !int(stm)* 384 + (int)place * 64 + int(to ^ 56);

        if (captured != PieceType::NONE)
        {
            Square capturedPieceSq = moveFlag == Move::EN_PASSANT_FLAG 
                                     ? (stm == Color::WHITE ? to - 8 : to + 8)
                                     : to;

            // Remove captured piece
            int sub2WhiteIdx = !int(stm) * 384 + (int)captured * 64 + (int)capturedPieceSq;
            int sub2BlackIdx = (int)stm * 384 + (int)captured * 64 + int(capturedPieceSq ^ 56);

            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
            {
                white[i] = oldAcc.white[i] 
                           - NET->featureWeights[sub1WhiteIdx][i]  // Remove piece from origin
                           - NET->featureWeights[sub2WhiteIdx][i]  // Remove captured piece
                           + NET->featureWeights[add1WhiteIdx][i]; // Put piece on destination

                black[i] = oldAcc.black[i] 
                           - NET->featureWeights[sub1BlackIdx][i]  // Remove piece from origin
                           - NET->featureWeights[sub2BlackIdx][i]  // Remove captured piece
                           + NET->featureWeights[add1BlackIdx][i]; // Put piece on destination
            }
        }
        else if (moveFlag == Move::CASTLING_FLAG)
        {
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];

            // Remove rook from origin
            int sub2WhiteIdx = (int)stm * 384 + (int)PieceType::ROOK * 64 + (int)rookFrom;
            int sub2BlackIdx = !int(stm) * 384 + (int)PieceType::ROOK * 64 + int(rookFrom ^ 56);

            // Place rook on destination
            int add2WhiteIdx = (int)stm * 384 + (int)PieceType::ROOK * 64 + (int)rookTo;
            int add2BlackIdx = !int(stm) * 384 + (int)PieceType::ROOK * 64 + int(rookTo ^ 56);

            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
            {
                white[i] = oldAcc.white[i] 
                           - NET->featureWeights[sub1WhiteIdx][i]  // Remove king from origin
                           + NET->featureWeights[add1WhiteIdx][i]  // Put king on destination
                           - NET->featureWeights[sub2WhiteIdx][i]  // Remove rook from origin
                           + NET->featureWeights[add2WhiteIdx][i]; // Put rook on destination

                black[i] = oldAcc.black[i] 
                           - NET->featureWeights[sub1BlackIdx][i]  // Remove king from origin
                           + NET->featureWeights[add1BlackIdx][i]  // Put king on destination
                           - NET->featureWeights[sub2BlackIdx][i]  // Remove rook from origin
                           + NET->featureWeights[add2BlackIdx][i]; // Put rook on destination
            }           
        }
        else {
            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
            {
                white[i] = oldAcc.white[i] 
                           - NET->featureWeights[sub1WhiteIdx][i]  // Remove piece from origin
                           + NET->featureWeights[add1WhiteIdx][i]; // Put piece on destination

                black[i] = oldAcc.black[i] 
                           - NET->featureWeights[sub1BlackIdx][i]  // Remove piece from origin
                           + NET->featureWeights[add1BlackIdx][i]; // Put piece on destination
            }        
        }

        updated = true;
    }

}; // struct Accumulator

inline i32 evaluate(Accumulator &accumulator, Board &board, bool materialScale)
{
    assert(accumulator.updated);

    Vec *stmAccumulator, *oppAccumulator;
    if (board.sideToMove() == Color::WHITE) {
        stmAccumulator = (Vec*)&accumulator.white;
        oppAccumulator = (Vec*)&accumulator.black;
    }
    else {
        stmAccumulator = (Vec*)&accumulator.black;
        oppAccumulator = (Vec*)&accumulator.white;
    }

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
        i32 material = 3 * std::popcount(board.bitboard(PieceType::KNIGHT))
                     + 3 * std::popcount(board.bitboard(PieceType::BISHOP))
                     + 5 * std::popcount(board.bitboard(PieceType::ROOK))
                     + 9 * std::popcount(board.bitboard(PieceType::QUEEN));

        constexpr i32 MATERIAL_MAX = 62;

        // Linear lerp from evalMaterialScaleMin to evalMaterialScaleMax as material goes from 0 to MATERIAL_MAX
        eval *= evalMaterialScaleMin.value + (evalMaterialScaleMax.value - evalMaterialScaleMin.value) * material / MATERIAL_MAX;
    }

    return std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

} // namespace nnue

using Accumulator = nnue::Accumulator;
Accumulator START_POS_ACCUMULATOR = Accumulator(START_BOARD);
