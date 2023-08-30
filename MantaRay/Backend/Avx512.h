//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_AVX512_H
#define MANTARAY_AVX512_H

#include "Avx2.h"

namespace MantaRay
{

    /// \brief AVX512 Intrinsics wrapper.
    /// \tparam T Type of the data.
    /// \details This class is a wrapper for AVX512 intrinsics. It provides a common interface for most types of data
    ///          used in MantaRay.
    template<typename T>
    class Avx512
    {

        static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>,
                      "Unsupported type provided.");

        public:
            /// \brief Load an AVX512 register with zeros.
            /// \return An AVX512 register filled with zeros of the type T.
            static inline Vec512I Zero()
            {
                return _mm512_setzero_si512();
            }

            /// \brief Load an AVX512 register with the provided value.
            /// \param value The value to load into the register.
            /// \return An AVX512 register loaded with the provided value.
            /// \details This function loads the provided value of type T into an AVX512 register, duplicating the value
            ///          across the register as necessary.
            static inline Vec512I From(const T value)
            {
                if (std::is_same_v<T, int8_t> ) return _mm512_set1_epi8 (value);

                if (std::is_same_v<T, int16_t>) return _mm512_set1_epi16(value);

                if (std::is_same_v<T, int32_t>) return _mm512_set1_epi32(value);
            }

            /// \brief Load an AVX512 register from an array.
            /// \tparam Size The size of the array.
            /// \param array The array to load from.
            /// \param index The index to begin loading from.
            /// \return An AVX512 register loaded from the array starting at the provided index.
            /// \details This function loads the provided array of type T into an AVX512 register, starting at the
            ///          provided index. The array must have at least 32 bytes of memory allocated starting from the
            ///          provided index.
            template<size_t Size>
            static inline Vec512I From(const std::array<T, Size>& array, const uint32_t index)
            {
                return _mm512_load_si512((Vec512I const*) &array[index]);
            }

            /// \brief Store an AVX512 register into an array.
            /// \tparam Size The size of the array.
            /// \param ymm0 The AVX512 register to store.
            /// \param array The array to store into.
            /// \param index The index to begin storing at.
            /// \details This function stores the provided AVX512 register into the provided array of type T, starting
            ///          at the provided index. The array must have at least 32 bytes of memory allocated starting from
            ///          the provided index.
            template<size_t Size>
            static inline void Store(const Vec512I& zmm0, std::array<T, Size>& array, const uint32_t index)
            {
                _mm512_store_si512((Vec512I *) &array[index], zmm0);
            }

            /// \brief Get a register with the minimum cross-register values of the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the minimum values of the two provided registers.
            /// \details This function returns a register with the minimum values of the two provided registers. The
            ///          minimum values are determined by comparing the values of the two registers at each index and
            ///          storing the minimum value at that index in the returned register. The returned register will
            ///          contain the minimum values of the two provided registers at each index.
            static inline Vec512I Min(const Vec512I& zmm0, const Vec512I& zmm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm512_min_epi8 (zmm0, zmm1);

                if (std::is_same_v<T, int16_t>) return _mm512_min_epi16(zmm0, zmm1);

                if (std::is_same_v<T, int32_t>) return _mm512_min_epi32(zmm0, zmm1);
            }

            /// \brief Get a register with the maximum cross-register values of the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the maximum values of the two provided registers.
            /// \details This function returns a register with the maximum values of the two provided registers. The
            ///          maximum values are determined by comparing the values of the two registers at each index and
            ///          storing the maximum value at that index in the returned register. The returned register will
            ///          contain the maximum values of the two provided registers at each index.
            static inline Vec512I Max(const Vec512I& zmm0, const Vec512I& zmm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm512_max_epi8 (zmm0, zmm1);

                if (std::is_same_v<T, int16_t>) return _mm512_max_epi16(zmm0, zmm1);

                if (std::is_same_v<T, int32_t>) return _mm512_max_epi32(zmm0, zmm1);
            }

            /// \brief Vertically add the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the sum of the two provided registers.
            /// \details This function returns a register with the sum of the two provided registers. The sum is
            ///          determined by adding the values of the two registers at each index and storing the sum at that
            ///          index in the returned register. The returned register will contain the sum of the two provided
            ///          registers at each index.
            static inline Vec512I Add(const Vec512I& zmm0, const Vec512I& zmm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm512_add_epi8 (zmm0, zmm1);

                if (std::is_same_v<T, int16_t>) return _mm512_add_epi16(zmm0, zmm1);

                if (std::is_same_v<T, int32_t>) return _mm512_add_epi32(zmm0, zmm1);
            }

            /// \brief Vertically subtract the two provided registers.
            /// \param ymm0 The first register.
            /// \param ymm1 The second register.
            /// \return A register with the difference of the two provided registers.
            /// \details This function returns a register with the difference of the two provided registers. The
            ///          difference is determined by subtracting the values of the two registers at each index and
            ///          storing the difference at that index in the returned register. The returned register will
            ///          contain the difference of the two provided registers at each index.
            static inline Vec512I Subtract(const Vec512I& zmm0, const Vec512I& zmm1)
            {
                if (std::is_same_v<T, int8_t> ) return _mm512_sub_epi8 (zmm0, zmm1);

                if (std::is_same_v<T, int16_t>) return _mm512_sub_epi16(zmm0, zmm1);

                if (std::is_same_v<T, int32_t>) return _mm512_sub_epi32(zmm0, zmm1);
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
            static inline Vec512I MultiplyAndAddAdjacent(const Vec512I& zmm0, const Vec512I& zmm1)
            {
                static_assert(std::is_same_v<T, int16_t>, "Unsupported type provided.");

                return _mm512_madd_epi16(zmm0, zmm1);
            }

            /// \brief Horizontally add the values of the provided register.
            /// \param ymm0 The register.
            /// \return The sum of the values of the provided register.
            /// \details This function returns the sum of the values of the provided register. The sum is determined by
            ///          converting the provided AVX512 register to two AVX2 registers, vertically adding them, and
            ///          then calculating the sum of the resulting AVX2 register.
            /// \see MantaRay::Backend::Avx2::Add(const Vec256I& ymm0, const Vec256I& ymm1)
            /// \see MantaRay::Backend::Avx2::Sum(const Vec256I& ymm0)
            static inline T Sum(const Vec512I& zmm0)
            {
                static_assert(std::is_same_v<T, int32_t>, "Unsupported type provided.");

                // Define the registers used in the horizontal addition:
                Vec256I ymm0;
                Vec256I ymm1;

                // Get the lower and upper half of the register:
                ymm0 = _mm512_castsi512_si256(zmm0);
                ymm1 = _mm512_extracti64x4_epi64(zmm0, 1);

                // Add the lower and upper half vertically:
                ymm0 = Avx2<T>::Add(ymm0, ymm1);

                // Rely on the AVX2 implementation to calculate the horizontal addition:
                return Avx2<T>::Sum(ymm0);
            }

    };

} // MantaRay

#endif //MANTARAY_AVX512_H
