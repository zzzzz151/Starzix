// clang-format off

#pragma once

#include <cstdint>
#include <cassert>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using Bitboard = u64;

enum class ColorCode : i32 {
    Green = 32, Yellow = 33
};

enum class Square : i8 {
    A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63,
    A7 = 48, B7 = 49, C7 = 50, D7 = 51, E7 = 52, F7 = 53, G7 = 54, H7 = 55,
    A6 = 40, B6 = 41, C6 = 42, D6 = 43, E6 = 44, F6 = 45, G6 = 46, H6 = 47,
    A5 = 32, B5 = 33, C5 = 34, D5 = 35, E5 = 36, F5 = 37, G5 = 38, H5 = 39,
    A4 = 24, B4 = 25, C4 = 26, D4 = 27, E4 = 28, F4 = 29, G4 = 30, H4 = 31,
    A3 = 16, B3 = 17, C3 = 18, D3 = 19, E3 = 20, F3 = 21, G3 = 22, H3 = 23,
    A2 = 8,  B2 = 9,  C2 = 10, D2 = 11, E2 = 12, F2 = 13, G2 = 14, H2 = 15,
    A1 = 0,  B1 = 1,  C1 = 2,  D1 = 3,  E1 = 4,  F1 = 5,  G1 = 6,  H1 = 7,
    Count = 64
};

enum class Color : i8 {
    White = 0, Black = 1, Count = 2
};

enum class PieceType : i8 {
    Pawn = 0, Knight = 1, Bishop = 2, Rook = 3, Queen = 4, King = 5, Count = 6
};

enum class File : i8 {
    A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7, Count = 8
};

enum class Rank : i8 {
    Rank1 = 0, Rank2 = 1, Rank3 = 2, Rank4 = 3,
    Rank5 = 4, Rank6 = 5, Rank7 = 6, Rank8 = 7,
    Count = 8
};

constexpr Color operator!(const Color color)
{
    assert(color != Color::Count);
    return color == Color::White ? Color::Black : Color::White;
}
