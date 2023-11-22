#pragma once

// clang-format off
#include <cstdint>
#include <string>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using Square = u8;

const inline i32 I32_MAX = 2147483647;
const inline u64 U64_MAX = 9223372036854775807;

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

const Square SQUARE_NONE = 255;

enum class Color : i8
{
    WHITE = 0,
    BLACK = 1,
    NONE = -1
};

enum class PieceType : u8
{
    PAWN   = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK   = 3,
    QUEEN  = 4,
    KING   = 5,
    NONE   = 6
};

enum class Piece : u8
{
    WHITE_PAWN   = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK   = 3,
    WHITE_QUEEN  = 4,
    WHITE_KING   = 5,
    BLACK_PAWN   = 6,
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8,
    BLACK_ROOK   = 9,
    BLACK_QUEEN  = 10,
    BLACK_KING   = 11,
    NONE         = 12
};

enum class Rank : u8
{
    RANK_1 = 0,
    RANK_2 = 1,
    RANK_3 = 2,
    RANK_4 = 3,
    RANK_5 = 4,
    RANK_6 = 5,
    RANK_7 = 6,
    RANK_8 = 7
};

enum class File : u8
{
    A = 0,
    B = 1,
    C = 2,
    D = 3,
    E = 4,
    F = 5,
    G = 6,
    H = 7
};

const i16 POS_INFINITY = 32000, 
          NEG_INFINITY = -POS_INFINITY, 
          MIN_MATE_SCORE = POS_INFINITY - 255;
