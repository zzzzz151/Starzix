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

constexpr i32 HIDDEN_LAYER_SIZE = 1024,
              SCALE = 400, 
              QA = 255, 
              QB = 64,
              WEIGHTS_PER_VEC = sizeof(Vec) / sizeof(i16);

struct alignas(ALIGNMENT) Net {
    public:
    MultiArray<i16, 2, 768, HIDDEN_LAYER_SIZE> featuresWeights; // [color][feature][hiddenNeuron]
    MultiArray<i16, 2, HIDDEN_LAYER_SIZE>      hiddenBiases;    // [color][hiddenNeuron]
    MultiArray<i16, 2, HIDDEN_LAYER_SIZE>      outputWeights;   // [color][hiddenNeuron]
    i16                                        outputBias;
};

INCBIN(NetFile, "src/net.bin");
const Net* NET = (const Net*)gNetFileData;

struct alignas(ALIGNMENT) Accumulator
{
    public:

    MultiArray<i16, 2, HIDDEN_LAYER_SIZE> mAccumulators;
    bool mUpdated = false;

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

        mUpdated = true;
    }

    inline void resetAccumulator(const Color color, Board &board) 
    {
        mAccumulators[(int)color] = NET->hiddenBiases[(int)color];

        for (Color pieceColor : {Color::WHITE, Color::BLACK})
            for (int pt = PAWN; pt <= KING; pt++)
            {
                u64 bb = board.getBb(pieceColor, (PieceType)pt);

                while (bb > 0) {
                    int square = poplsb(bb);

                    const int ft = feature(color, pieceColor, (PieceType)pt, square);

                    for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                        mAccumulators[(int)color][i] += NET->featuresWeights[(int)color][ft][i];
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

        Move move = board.lastMove();
        assert(move != MOVE_NONE);

        const Color stm = board.oppSide(); // side that moved
        const int from = move.from();
        const int to = move.to();
        const PieceType pieceType = move.pieceType();
        const PieceType promotion = move.promotion();
        const PieceType place = promotion != PieceType::NONE ? promotion : pieceType;

        // If a king moved, we update mMirrorHorizontally[stm]
        if (pieceType == PieceType::KING)
        {
            File newKingFile = squareFile(board.kingSquare(stm));
            mMirrorHorizontally[(int)stm] = (int)newKingFile >= (int)File::E;

            // If the king crossed the vertical axis, we reset his color's accumulator
            if (mMirrorHorizontally[(int)stm] != oldAcc->mMirrorHorizontally[(int)stm])
                resetAccumulator(stm, board);
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
            // due to that king crossing the vertical axis
            if (mMirrorHorizontally[color] != oldAcc->mMirrorHorizontally[color]) 
                continue;

            if (board.captured() != PieceType::NONE)
            {
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][addPieceFeature[color]][i]
                        - NET->featuresWeights[color][subCapturedFeature[color]][i];
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
                        - NET->featuresWeights[color][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][addPieceFeature[color]][i]
                        - NET->featuresWeights[color][subRookFeature[color]][i]  
                        + NET->featuresWeights[color][addRookFeature[color]][i];
            }
            else {
                for (int i = 0; i < HIDDEN_LAYER_SIZE; i++) 
                    mAccumulators[color][i] = 
                        oldAcc->mAccumulators[color][i]
                        - NET->featuresWeights[color][subPieceFeature[color]][i]  
                        + NET->featuresWeights[color][addPieceFeature[color]][i];
            }
        }

        mUpdated = true;
    }

}; // struct Accumulator

inline i32 evaluate(Accumulator* accumulator, Board &board, bool materialScale)
{
    assert(accumulator->mUpdated);

    int stm = (int)board.sideToMove();
    Vec* stmAccumulator = (Vec*) &(accumulator->mAccumulators[stm]);
    Vec* oppAccumulator = (Vec*) &(accumulator->mAccumulators[!stm]);

    Vec* stmWeights = (Vec*) &(NET->outputWeights[0]);
    Vec* oppWeights = (Vec*) &(NET->outputWeights[1]);
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
