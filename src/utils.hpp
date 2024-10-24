// clang-format off

#pragma once

#include "array_extensions.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <bitset>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cmath>
#include <bit>

// Integer types

using size_t = size_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// General utils

#define stringify(myVar) (std::string)#myVar

#define ln(x) log(x)

inline void trim(std::string &str) {
    const size_t first = str.find_first_not_of(" \t\n\r");
    const size_t last = str.find_last_not_of(" \t\n\r");
    
    str = first == std::string::npos 
          ? "" 
          : str.substr(first, last - first + 1);
}

inline std::vector<std::string> splitString(std::string &str, const char delimiter)
{
    trim(str);
    if (str == "") return std::vector<std::string> { };

    std::vector<std::string> strSplit;
    std::stringstream ss(str);
    std::string token;

    while (getline(ss, token, delimiter)) 
    {
        trim(token);
        strSplit.push_back(token);
    }

    return strSplit;
}

constexpr int charToInt(const char myChar) { return myChar - '0'; }

inline u64 millisecondsElapsed(const std::chrono::steady_clock::time_point start)
{
    return (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
}

// Square

using Square = u8;
constexpr Square SQUARE_NONE = 255;

const std::string SQUARE_TO_STR[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

inline Square strToSquare(const std::string strSquare) {
    return (strSquare[0] - 'a') + (strSquare[1] - '1') * 8;
}

// Color

enum class Color : i8 {
    WHITE = 0, BLACK = 1
};

constexpr int WHITE = 0, BLACK = 1;

constexpr Color oppColor(const Color color) { 
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

// Piece type

enum class PieceType : u8 {
    PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NONE = 6
};

constexpr int PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5;

// Rank and file

enum class Rank : u8 {
    RANK_1 = 0, RANK_2 = 1, RANK_3 = 2, RANK_4 = 3,
    RANK_5 = 4, RANK_6 = 5, RANK_7 = 6, RANK_8 = 7
};

enum class File : u8 {
    A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7
};

constexpr Rank squareRank(const Square square) { return Rank(square / 8); }

constexpr File squareFile(const Square square) { return File(square % 8); }

// Bitboards

constexpr u64 ONES_BB = 0xffff'ffff'ffff'ffff;

constexpr std::array<u64, 8> RANK_BB = {
    0xffULL, 0xff00ULL, 0xff0000ULL, 0xff000000ULL, 
    0xff00000000ULL, 0xff0000000000ULL, 0xff000000000000ULL, 0xff00000000000000ULL
};

constexpr std::array<u64, 8> FILE_BB = {
    0x101010101010101ULL, 0x202020202020202ULL, 0x404040404040404ULL, 0x808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
};

constexpr u64 bitboard(const Square sq) 
{ 
    assert(sq >= 0 && sq <= 63);
    return 1ULL << sq; 
}

constexpr u8 lsb(const u64 bb) 
{
    assert(bb > 0);
    return std::countr_zero(bb);
}

constexpr u8 poplsb(u64 &bb)
{
    auto idx = lsb(bb);
    bb &= bb - 1; // compiler optimizes this to _blsr_u64
    return idx;
}

constexpr u8 msb(const u64 bb)
{
    assert(bb > 0);
    return 63 - __builtin_clzll(bb);
}

constexpr u8 popmsb(u64 &bb)
{
    auto idx = msb(bb);
    bb &= ~(1ULL << idx);
    return idx;
}

inline void printBitboard(const u64 bb)
{
    std::cout << std::endl;
    const std::bitset<64> b(bb);
    const std::string str_bitset = b.to_string(); 

    for (int i = 0; i < 64; i += 8)
    {
        std::string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());

        for (u64 j = 0; j < x.length(); j++)
            std::cout << std::string(1, x[j]) << " ";

        std::cout << std::endl;
    }
}

constexpr u64 pdep(const u64 val, u64 mask) {
    u64 res = 0;

    for (u64 bb = 1; mask; bb += bb) 
    {
        if (val & bb)
            res |= mask & -mask;

        mask &= mask - 1;
    }

    return res;
}

// Castling

// [color][isLongCastle]
constexpr MultiArray<u64, 2, 2> CASTLING_MASKS = {{
    // White short and long castle
    { bitboard(7), bitboard(0) },
    // Black short and long castle
    { bitboard(63), bitboard(56) } 
}};

// [kingTargetSquare]
constexpr std::array<std::pair<Square, Square>, 64> CASTLING_ROOK_FROM_TO = []() consteval
{
    std::array<std::pair<Square, Square>, 64> castlingRookFromTo = { };

    castlingRookFromTo[6]  = { 7, 5 };   // White short castle
    castlingRookFromTo[2]  = { 0, 3 };   // White long castle
    castlingRookFromTo[62] = { 63, 61 }; // Black short castle
    castlingRookFromTo[58] = { 56, 59 }; // Black long castle

    return castlingRookFromTo;
}();

// Zobrist hashing

constexpr u64 nextU64(u64 &state)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717LL;
}

constexpr u64 ZOBRIST_COLOR = 591679071752537765ULL;

// [color][pieceType][square]
constexpr MultiArray<u64, 2, 6, 64> ZOBRIST_PIECES = []() consteval
{
    MultiArray<u64, 2, 6, 64> zobristPieces;
    u64 rngState = 35907035218217ULL;

    for (const int color : {WHITE, BLACK})
        for (int pieceType = PAWN; pieceType <= KING; pieceType++)
            for (Square sq = 0; sq < 64; sq++)
                zobristPieces[color][pieceType][sq] = nextU64(rngState);

    return zobristPieces;
}();

// [file]
constexpr std::array<u64, 8> ZOBRIST_FILES = {
    12228382040141709029ULL, 2494223668561036951ULL, 7849557628814744642ULL, 16000570245257669890ULL,
    16614404541835922253ULL, 17787301719840479309ULL, 6371708097697762807ULL, 7487338029351702425ULL,
};
