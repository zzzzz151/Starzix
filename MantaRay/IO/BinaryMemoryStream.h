//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_BINARYMEMORYSTREAM_H
#define MANTARAY_BINARYMEMORYSTREAM_H

namespace std
{

    template<>
    class ctype<unsigned char> : public ctype<char>
    {

        public:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
            ctype(size_t refs = 0) : ctype<char>(reinterpret_cast<const mask *>(refs)) {}
#pragma clang diagnostic pop

    };

}

#include <locale>
#include <streambuf>
#include <istream>
#include <array>
#include <cstdint>

#include "DataStream.h"

namespace MantaRay
{

    using streambuf = std::basic_streambuf<unsigned char, std::char_traits<unsigned char>>;

    /// \brief A stream buffer that encompasses a region of memory.
    /// \details This class is a stream buffer that encompasses a region of memory. It is used to read and write data
    ///          to and from memory.
    struct BinaryMemoryBuffer : streambuf
    {

        public:
            /// \brief The Buffer Constructor.
            /// \param src The source memory.
            /// \param size The size of the memory.
            /// \details This constructor is used to create a stream buffer that encompasses a region of memory.
            BinaryMemoryBuffer(const unsigned char* src, const size_t size) {
                auto *p (const_cast<unsigned char*>(src));
                this->setg(p, p, p + size);
            }

    };

    using istream = std::basic_istream<unsigned char, std::char_traits<unsigned char>>;

    /// \brief A stream that encompasses a region of memory.
    /// \details This class is a stream that encompasses a region of memory. It is used to read and write data to and
    ///          from memory.
    struct BinaryMemoryStream : virtual MantaRay::BinaryMemoryBuffer, istream
    {

        public:
            /// \brief The Stream Constructor.
            /// \param src The source memory.
            /// \param size The size of the memory.
            /// \details This constructor is used to create a stream that encompasses a region of memory.
            __attribute__((unused)) BinaryMemoryStream(const unsigned char* src, const size_t size) :
            BinaryMemoryBuffer(src, size), istream(static_cast<streambuf*>(this)) {}

            /// \brief Read an array from the memory encompassed by the stream.
            /// \tparam T The type of the array.
            /// \tparam Size The size of the array.
            /// \param array The array to read into.
            /// \details This method is used to read an array from the memory encompassed by the stream.
            template<typename T, size_t Size>
            void ReadArray(std::array<T, Size> &array)
            {
                this->read((unsigned char*)(&array), sizeof array);
            }

    };

} // MantaRay

#endif //MANTARAY_BINARYMEMORYSTREAM_H
