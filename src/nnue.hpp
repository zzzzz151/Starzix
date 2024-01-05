#pragma once

#ifdef _MSC_VER
#define STARZIX_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "incbin.h"

// clang-format off

namespace nnue {

const u16 HIDDEN_LAYER_SIZE = 512;
const i32 SCALE = 400, QA = 255, QB = 64;

struct alignas(64) NN {
    std::array<i16, 768 * HIDDEN_LAYER_SIZE> featureWeights;
    std::array<i16, HIDDEN_LAYER_SIZE> featureBiases;
    std::array<i16, HIDDEN_LAYER_SIZE * 2> outputWeights;
    i16 outputBias;
};

INCBIN(NetFile, "src/net6.nnue");
const NN *nn = reinterpret_cast<const NN*>(gNetFileData);

struct Accumulator
{
    i16 white[HIDDEN_LAYER_SIZE];
    i16 black[HIDDEN_LAYER_SIZE];

    inline Accumulator()
    {
        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            white[i] = black[i] = nn->featureBiases[i];
    }

    inline void update(Color color, PieceType pieceType, Square sq, bool activate)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + sq;
        int blackIdx = !(int)color * 384 + (int)pieceType * 64 + (sq ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            if (activate)
            {
                white[i] += nn->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];
                black[i] += nn->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
            }
            else
            {
                white[i] -= nn->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];
                black[i] -= nn->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
            }
        }
    }   
};

inline i32 crelu(i16 x) {
    return x < 0 ? 0 : x > QA ? QA : x;
}

inline i32 screlu(i32 x) {
    i32 clamped = std::clamp(x, 0, QA);
    return clamped * clamped;
}

inline i16 evaluate(Accumulator &accumulator, Color color)
{
    i16 *us = accumulator.white,
        *them = accumulator.black;

    if (color == Color::BLACK)
    {
        us = accumulator.black;
        them = accumulator.white;
    }

    i32 sum = 0;
    for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
    {
        sum += screlu(us[i]) * nn->outputWeights[i];
        sum += screlu(them[i]) * nn->outputWeights[HIDDEN_LAYER_SIZE + i];
    }

    i32 eval = (sum / QA + nn->outputBias) * SCALE / (QA * QB);
    return std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

}

using Accumulator = nnue::Accumulator;
