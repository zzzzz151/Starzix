//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_AVX_H
#define MANTARAY_AVX_H

#include <cstdint>
#include <array>
#include "RegisterDefinition.h"

namespace MantaRay
{

    /// \brief AVX Intrinsics wrapper.
    /// \tparam T Type of the data.
    /// \details This class is a wrapper for AVX intrinsics. It provides a common interface for most types of data used
    ///          in MantaRay.
    template<typename T>
    class Avx
    {

        static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t>,
                "Unsupported type provided.");

        public:
            /// \brief Load an AVX register with zeros.
            /// \return An AVX register filled with zeros of the type T.
            static inline Vec256I Zero()
            {
                return _mm256_setzero_si256();
            }

            /// \brief Load an AVX register with the provided value.
            /// \param value The value to load into the register.
            /// \return An AVX register loaded with the provided value.
            /// \details This function loads the provided value of type T into an AVX register, duplicating the value
            ///          across the register as necessary.
            static inline Vec256I From(const T value)
            {
                if (std::is_same_v<T, int8_t> ) return _mm256_set1_epi8 (value);

                if (std::is_same_v<T, int16_t>) return _mm256_set1_epi16(value);

                if (std::is_same_v<T, int32_t>) return _mm256_set1_epi32(value);
            }

            /// \brief Load an AVX register from an array.
            /// \tparam Size The size of the array.
            /// \param array The array to load from.
            /// \param index The index to begin loading from.
            /// \return An AVX register loaded from the array starting at the provided index.
            /// \details This function loads the provided array of type T into an AVX register, starting at the provided
            ///          index. The array must have at least 32 bytes of memory allocated starting from the provided
            ///          index.
            template<size_t Size>
            static inline Vec256I From(const std::array<T, Size>& array, const uint32_t index)
            {
                static_assert(sizeof(array) >= 32, "Array must be at least 32 bytes in size.");

                return _mm256_load_si256((Vec256I const*) &array[index]);
            }

            /// \brief Store an AVX register into an array.
            /// \tparam Size The size of the array.
            /// \param ymm0 The AVX register to store.
            /// \param array The array to store into.
            /// \param index The index to begin storing at.
            /// \details This function stores the provided AVX register into the provided array of type T, starting at
            ///          the provided index. The array must have at least 32 bytes of memory allocated starting from the
            ///          provided index.
            template<size_t Size>
            static inline void Store(const Vec256I& ymm0, std::array<T, Size>& array, const uint32_t index)
            {
                static_assert(sizeof(array) >= 32, "Array must be at least 32 bytes in size.");

                _mm256_store_si256((Vec256I *) &array[index], ymm0);
            }

    };

} // MantaRay

#endif //MANTARAY_AVX_H
