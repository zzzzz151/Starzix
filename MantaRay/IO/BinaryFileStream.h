//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_BINARYFILESTREAM_H
#define MANTARAY_BINARYFILESTREAM_H

#include <array>
#include <cstdint>
#include "DataStream.h"

namespace MantaRay
{

    /// \brief A binary file stream.
    /// \details This class is a wrapper around std::fstream that provides a binary view of the file.
    class BinaryFileStream : DataStream<std::ios::binary | std::ios::in>
    {

        private:
            std::string Path;

        public:
            /// \brief The BinaryFileStream constructor.
            /// \param path The path to the file.
            /// \details This constructor opens the file in binary read mode.
            __attribute__((unused)) explicit BinaryFileStream(const std::string& path) : DataStream(path)
            {
                Path = path;
            }

            /// \brief Convert the stream to write mode.
            /// \details This function closes the stream and reopens it in write mode.
            void WriteMode()
            {
                // Weird C++ issue workaround.
                this->Stream.close();
                this->Stream.open(Path, std::ios::binary | std::ios::out);
            }

            /// \brief Read an array from the stream.
            /// \tparam T The type of the array.
            /// \tparam Size The size of the array.
            /// \param array The array to read into.
            /// \details This function reads an array from the stream.
            template<typename T, size_t Size>
            void ReadArray(std::array<T, Size>& array)
            {
                this->Stream.read((char*)(&array), sizeof array);
            }

            /// \brief Write an array to the stream.
            /// \tparam T The type of the array.
            /// \tparam Size The size of the array.
            /// \param array The array to write.
            /// \details This function writes an array to the stream.
            template<typename T, size_t Size>
            void WriteArray(const std::array<T, Size>& array)
            {
                this->Stream.write((const char*)(&array), sizeof array);
            }

    };

} // MantaRay

#endif //MANTARAY_BINARYFILESTREAM_H
