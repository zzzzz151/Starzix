// clang-format off

#pragma once

#include "utils.hpp"
#include <immintrin.h>

#if (defined(__AVX512F__) && defined(__AVX512BW__)) || defined(__AVX2__)

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        using Vec = __m512i;
    #else // AVX2
        using Vec = __m256i;
    #endif

    constexpr Vec setEpi16(const i16 x)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_set1_epi16(x);
        #else // AVX2
            return _mm256_set1_epi16(x);
        #endif
    }

    constexpr Vec loadVec(const Vec* vecPtr)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_load_si512(vecPtr);
        #else // AVX2
            return _mm256_load_si256(vecPtr);
        #endif
    }

    constexpr Vec clampVec(const Vec vec, const Vec minVec, const Vec maxVec)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_min_epi16(_mm512_max_epi16(vec, minVec), maxVec);
        #else // AVX2
            return _mm256_min_epi16(_mm256_max_epi16(vec, minVec), maxVec);
        #endif
    }

    constexpr Vec mulloEpi16(const Vec a, const Vec b)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_mullo_epi16(a, b);
        #else // AVX2
            return _mm256_mullo_epi16(a, b);
        #endif
    }

    constexpr Vec maddEpi16(const Vec a, const Vec b)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_madd_epi16(a, b);
        #else // AVX2
            return _mm256_madd_epi16(a, b);
        #endif
    }

    constexpr Vec addEpi32(const Vec a, const Vec b)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_add_epi32(a, b);
        #else // AVX2
            return _mm256_add_epi32(a, b);
        #endif
    }

    // Adds the i16's in vec, returning an i32
    inline i32 sumVec(const Vec vec)
    {
        #if defined(__AVX512F__) && defined(__AVX512BW__)
            return _mm512_reduce_add_epi32(vec);
        #else // AVX2
            // Get the lower and upper half of the register:
            __m128i xmm0 =  _mm256_castsi256_si128(vec);
            __m128i xmm1 =  _mm256_extracti128_si256(vec, 1);

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
        #endif
    }
#else
    using Vec = __m128i;
#endif
