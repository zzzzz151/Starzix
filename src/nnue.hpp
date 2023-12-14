#pragma once

#ifdef _MSC_VER
#define Z5_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "incbin.h"

// clang-format off

namespace nnue {

const u16 HIDDEN_LAYER_SIZE = 384;
const i32 SCALE = 400, 
          Q = 255 * 64, 
          NORMALIZATION_K = 1;

struct alignas(64) NN {
    std::array<i16, 768 * HIDDEN_LAYER_SIZE> featureWeights;
    std::array<i16, HIDDEN_LAYER_SIZE> featureBiases;
    std::array<i8, HIDDEN_LAYER_SIZE * 2> outputWeights;
    i16 outputBias;
};

INCBIN(NetFile, "src/net.nnue");
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
        int whiteOffset = whiteIdx * HIDDEN_LAYER_SIZE;
        int blackOffset = blackIdx * HIDDEN_LAYER_SIZE;

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            if (activate)
            {
                white[i] += nn->featureWeights[whiteOffset + i];
                black[i] += nn->featureWeights[blackOffset + i];
            }
            else
            {
                white[i] -= nn->featureWeights[whiteOffset + i];
                black[i] -= nn->featureWeights[blackOffset + i];
            }
        }
    }   
};

std::vector<Accumulator> accumulators;
Accumulator *currentAccumulator;

inline void reset()
{
    accumulators.clear();
    accumulators.reserve(256);
    accumulators.push_back(Accumulator());
    currentAccumulator = &accumulators.back();
}

inline void push()
{
    assert(currentAccumulator == &accumulators.back());
    accumulators.push_back(*currentAccumulator);
    currentAccumulator = &accumulators.back();
}

inline void pull()
{
    accumulators.pop_back();
    currentAccumulator = &accumulators.back();
}

inline i32 crelu(i32 x) {
    return std::clamp(x, 0, 255);
}

inline i32 evaluate(Color color)
{
    i16 *us = currentAccumulator->white,
        *them = currentAccumulator->black;

    if (color == Color::BLACK)
    {
        us = currentAccumulator->black;
        them = currentAccumulator->white;
    }

    i32 sum = 0;
    for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
    {
        sum += crelu(us[i]) * nn->outputWeights[i];
        sum += crelu(them[i]) * nn->outputWeights[HIDDEN_LAYER_SIZE + i];
    }

    return (sum / NORMALIZATION_K + nn->outputBias) * SCALE / Q;
}

}

