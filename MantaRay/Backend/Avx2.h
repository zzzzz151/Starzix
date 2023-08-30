//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_AVX2_H
#define MANTARAY_AVX2_H

#include "Avx.h"

namespace MantaRay
{

    /// \brief AVX2 Intrinsics wrapper.
    /// \tparam T Type of the data.
    /// \details This class is a wrapper for AVX2 intrinsics. It provides a common interface for most types of data used
    ///          in MantaRay.
    template<typename T>
    class Avx2
    {

        static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>,
                      "Unsupported type provided.");

        public:
            /// \brief Get a register with the minimum cross-register values of the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the minimum values of the two provided registers.
            /// \details This function returns a register with the minimum values of the two provided registers. The
            ///          minimum values are determined by comparing the values of the two registers at each index and
            ///          storing the minimum value at that index in the returned register. The returned register will
            ///          contain the minimum values of the two provided registers at each index.
            static inline Vec256I Min(const Vec256I& ymm0, const Vec256I& ymm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm256_min_epi8 (ymm0, ymm1);

                if (std::is_same_v<T, int16_t>) return _mm256_min_epi16(ymm0, ymm1);

                if (std::is_same_v<T, int32_t>) return _mm256_min_epi32(ymm0, ymm1);
            }

            /// \brief Get a register with the maximum cross-register values of the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the maximum values of the two provided registers.
            /// \details This function returns a register with the maximum values of the two provided registers. The
            ///          maximum values are determined by comparing the values of the two registers at each index and
            ///          storing the maximum value at that index in the returned register. The returned register will
            ///          contain the maximum values of the two provided registers at each index.
            static inline Vec256I Max(const Vec256I& ymm0, const Vec256I& ymm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm256_max_epi8 (ymm0, ymm1);

                if (std::is_same_v<T, int16_t>) return _mm256_max_epi16(ymm0, ymm1);

                if (std::is_same_v<T, int32_t>) return _mm256_max_epi32(ymm0, ymm1);
            }

            /// \brief Vertically add the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the sum of the two provided registers.
            /// \details This function returns a register with the sum of the two provided registers. The sum is
            ///          determined by adding the values of the two registers at each index and storing the sum at that
            ///          index in the returned register. The returned register will contain the sum of the two provided
            ///          registers at each index.
            static inline Vec256I Add(const Vec256I& ymm0, const Vec256I& ymm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm256_add_epi8 (ymm0, ymm1);

                if (std::is_same_v<T, int16_t>) return _mm256_add_epi16(ymm0, ymm1);

                if (std::is_same_v<T, int32_t>) return _mm256_add_epi32(ymm0, ymm1);
            }

            /// \brief Vertically subtract the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the difference of the two provided registers.
            /// \details This function returns a register with the difference of the two provided registers. The
            ///          difference is determined by subtracting the values of the two registers at each index and
            ///          storing the difference at that index in the returned register. The returned register will
            ///          contain the difference of the two provided registers at each index.
            static inline Vec256I Subtract(const Vec256I& ymm0, const Vec256I& ymm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm256_sub_epi8 (ymm0, ymm1);

                if (std::is_same_v<T, int16_t>) return _mm256_sub_epi16(ymm0, ymm1);

                if (std::is_same_v<T, int32_t>) return _mm256_sub_epi32(ymm0, ymm1);
            }

            /// \brief Multiply the two provided registers and add the values at adjacent indices.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the sum of the products of the two provided registers.
            /// \details This function returns a register with the sum of the products of the two provided registers.
            ///          The products are determined by multiplying the values of the two registers at each index and
            ///          then summing it up with the product of the values at the adjacent index. The returned register
            ///          will be of a different type than the provided registers: doubling the bits of the provided
            ///          type.
            static inline Vec256I MultiplyAndAddAdjacent(const Vec256I& ymm0, const Vec256I& ymm1)
            {
                static_assert(std::is_same_v<T, int16_t>, "Unsupported type provided.");

                return _mm256_madd_epi16(ymm0, ymm1);
            }

            /// \brief Horizontally add the values of the provided register.
            /// \param ymm0 The register.
            /// \return The sum of the values of the provided register.
            /// \details This function returns the sum of the values of the provided register. The sum is determined by
            ///          accumulating the values in the register.
            static inline T Sum(const Vec256I& ymm0)
            {
                static_assert(std::is_same_v<T, int32_t>, "Unsupported type provided.");

                // Define the registers used in the horizontal addition:
                Vec128I xmm0;
                Vec128I xmm1;

                // Get the lower and upper half of the register:
                xmm0 = _mm256_castsi256_si128(ymm0);
                xmm1 = _mm256_extracti128_si256(ymm0, 1);

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

    };

} // MantaRay

#endif //MANTARAY_AVX2_H
