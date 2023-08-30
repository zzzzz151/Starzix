//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_MARLINFLOWSTREAM_H
#define MANTARAY_MARLINFLOWSTREAM_H

#include <array>
#include <cstdint>
#include "DataStream.h"
#include "../External/json.hpp"

namespace MantaRay
{

    /// \brief A Marlinflow JSON stream.
    /// \details This class is used to read Marlinflow JSON network data.
    class MarlinflowStream : DataStream<std::ios::in>
    {

        private:
            using JSON = nlohmann::json;

            JSON data;

        public:
            /// \brief The constructor of the MarlinflowStream.
            /// \param path The path to the Marlinflow JSON file.
            /// \details This constructor will read the Marlinflow JSON file and parse it.
            __attribute__((unused)) explicit MarlinflowStream(const std::string& path) : DataStream(path)
            {
                data = JSON::parse(this->Stream);
            }

            /// \brief Read a 2D array from the Marlinflow JSON file.
            /// \tparam T The type of the array.
            /// \tparam Size The size of the array.
            /// \param key The key of the array.
            /// \param array The array to read into.
            /// \param stride The stride of the array.
            /// \param K The Quantization factor.
            /// \param permute Whether to permute the array.
            /// \details This function will read a 2D array from the Marlinflow JSON file, quantize it and permute it if
            ///          necessary, storing it in the provided array.
            template<typename T, size_t Size>
            void Read2DArray(const std::string& key, std::array<T, Size>& array, const size_t stride, const size_t K,
                             const bool permute)
            {
                JSON &obj = data[key];

                // Loop over the JSON object in a 2D fashion:
                size_t i = 0;
                for (auto &[k, v] : obj.items()) {
                    size_t j = 0;

                    for (auto &[k2, v2] : v.items()) {
                        // Calculate the 1D index with respect to the stride and permutation (if necessary):
                        size_t idx = permute ? j * stride + i : i * stride + j;

                        // Quantize the value and store it in the array:
                        auto d     = static_cast<double>(v2);
                        array[idx] = static_cast<T>(d * K);
                        j++;
                    }

                    i++;
                }
            }

            /// \brief Read a 1D array from the Marlinflow JSON file.
            /// \tparam T The type of the array.
            /// \tparam Size The size of the array.
            /// \param key The key of the array.
            /// \param array The array to read into.
            /// \param K The Quantization factor.
            /// \details This function will read a 1D array from the Marlinflow JSON file, quantize it and store it in
            ///          the provided array.
            template<typename T, size_t Size>
            void ReadArray(const std::string& key, std::array<T, Size>& array, const size_t K)
            {
                JSON &obj = data[key];

                // Loop over the JSON object in a 1D fashion:
                size_t i = 0;
                for (auto &[k, v] : obj.items()) {
                    // Quantize the value and store it in the array:
                    auto d   = static_cast<double>(v);
                    array[i] = static_cast<T>(d * K);
                    i++;
                }
            }

    };

} // MantaRay

#endif //MANTARAY_MARLINFLOWSTREAM_H
