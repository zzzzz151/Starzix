#pragma once

#include <immintrin.h> // Intrinsics
#include <algorithm>

#ifdef _MSC_VER
#define STARZIX_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif
#include "incbin.h"

// clang-format off

namespace nnue {

const u16 HIDDEN_LAYER_SIZE = 512;
const i32 SCALE = 400, QA = 181, QB = 64;

struct alignas(64) NN {
    std::array<i16, 768 * HIDDEN_LAYER_SIZE> featureWeights;
    std::array<i16, HIDDEN_LAYER_SIZE> featureBiases;
    std::array<i16, HIDDEN_LAYER_SIZE * 2> outputWeights;
    i16 outputBias;
};

INCBIN(NetFile, "src-test/net9.nnue");
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

    inline void activate(Color color, PieceType pieceType, Square sq)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + sq;
        int blackIdx = !(int)color * 384 + (int)pieceType * 64 + (sq ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            white[i] += nn->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            black[i] += nn->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
    }

    inline void deactivate(Color color, PieceType pieceType, Square sq)
    {
        int whiteIdx = (int)color * 384 + (int)pieceType * 64 + sq;
        int blackIdx = !(int)color * 384 + (int)pieceType * 64 + (sq ^ 56);

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            white[i] -= nn->featureWeights[whiteIdx * HIDDEN_LAYER_SIZE + i];

        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            black[i] -= nn->featureWeights[blackIdx * HIDDEN_LAYER_SIZE + i];
    }
};

inline i32 crelu(i16 x) {
    return x < 0 ? 0 : x > QA ? QA : x;
}

inline i32 screlu(i32 x) {
    i32 clamped = std::clamp(x, 0, QA);
    return clamped * clamped;
}

inline i32 vecHaddEpi32(__m256i vec) {
    __m128i xmm0;
    __m128i xmm1;

    // Get the lower and upper half of the register:
    xmm0 = _mm256_castsi256_si128(vec);
    xmm1 = _mm256_extracti128_si256(vec, 1);

    // Add the lower and upper half vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Get the upper half of the result:
    xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

    // Add the lower and upper half vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Shuffle the result so that the lower 32-bits are directly above the second-lower 32-bits:
    xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(2, 3, 0, 1));

    // Add the lower 32-bits to the second-lower 32-bits vertically:
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    // Cast the result to the 32-bit integer type and return it:
    return _mm_cvtsi128_si32(xmm0);
}


inline i32 evaluate(Accumulator &accumulator, Color color)
{
    __m256i* stmAccumulator;
    __m256i* oppAccumulator;

    if (color == Color::WHITE)
    {
        stmAccumulator = (__m256i*)accumulator.white;
        oppAccumulator = (__m256i*)accumulator.black;
    }
    else
    {
        stmAccumulator = (__m256i*)accumulator.black;
        oppAccumulator = (__m256i*)accumulator.white;
    }
    
    __m256i* stmWeights = (__m256i*)&nn->outputWeights[0];
    __m256i* oppWeights = (__m256i*)&nn->outputWeights[HIDDEN_LAYER_SIZE];

    const __m256i reluClipMin = _mm256_setzero_si256();
    const __m256i reluClipMax = _mm256_set1_epi16(QA);

    __m256i sum = _mm256_setzero_si256();
    __m256i reg;

    for (int i = 0; i < HIDDEN_LAYER_SIZE / 16; ++i) 
    {
      // Side to move
      reg = _mm256_max_epi16(stmAccumulator[i], reluClipMin); // clip
      reg = _mm256_min_epi16(reg, reluClipMax); // clip
      reg = _mm256_mullo_epi16(reg, reg); // square
      reg = _mm256_madd_epi16(reg, stmWeights[i]); // multiply with output layer
      sum = _mm256_add_epi32(sum, reg); // collect the result

      // Non side to move
      reg = _mm256_max_epi16(oppAccumulator[i], reluClipMin);
      reg = _mm256_min_epi16(reg, reluClipMax);
      reg = _mm256_mullo_epi16(reg, reg);
      reg = _mm256_madd_epi16(reg, oppWeights[i]);
      sum = _mm256_add_epi32(sum, reg);
    }

    i32 eval = (vecHaddEpi32(sum) / QA + nn->outputBias) * SCALE / (QA * QB);
    return std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

}

using Accumulator = nnue::Accumulator;
