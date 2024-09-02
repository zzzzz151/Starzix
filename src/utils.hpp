// clang-format off

#pragma once

#define stringify(myVar) (std::string)#myVar

#include "array_extensions.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <bitset>
#include <chrono>
#include <bit>
#include <cmath>
#include <type_traits>

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

constexpr i32 I32_MAX = 2147483647;
constexpr i64 I64_MAX = 9223372036854775807;

constexpr u64 ONES = 0xffff'ffff'ffff'ffff;

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

using Square = u8;
constexpr Square SQUARE_NONE = 255;

enum class Color : i8 {
    WHITE = 0, BLACK = 1
};

constexpr int WHITE = 0, BLACK = 1;

enum class PieceType : u8 {
    PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NONE = 6
};

constexpr int PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5;

enum class Rank : u8 {
    RANK_1 = 0, RANK_2 = 1, RANK_3 = 2, RANK_4 = 3,
    RANK_5 = 4, RANK_6 = 5, RANK_7 = 6, RANK_8 = 7
};

enum class File : u8 {
    A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7
};

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

constexpr Color oppColor(const Color color) { 
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

constexpr Rank squareRank(const Square square) { return (Rank)(square / 8); }

constexpr File squareFile(const Square square) { return (File)(square % 8); }

constexpr u64 bitboard(const Square sq) { 
    assert(sq >= 0 && sq <= 63);
    return 1ULL << sq; 
}

constexpr u8 lsb(const u64 bb) {
    assert(bb > 0);
    return std::countr_zero(bb);
}

constexpr u8 poplsb(u64 &bb)
{
    auto idx = lsb(bb);
    bb &= bb - 1; // compiler optimizes this to _blsr_u64
    return idx;
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

constexpr u64 shiftRight(const u64 bb) {
    return (bb << 1ULL) & 0xfefefefefefefefeULL;
}

constexpr u64 shiftLeft(const u64 bb) {
    return (bb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL;
}

constexpr u64 shiftUp(const u64 bb) { return bb << 8ULL; }

constexpr u64 shiftDown(const u64 bb) { return bb >> 8ULL; }

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

inline void trim(std::string &str) {
    const size_t first = str.find_first_not_of(" \t\n\r");
    const size_t last = str.find_last_not_of(" \t\n\r");
    
    str = first == std::string::npos 
          ? "" 
          : str.substr(first, (last - first + 1));
}

inline std::vector<std::string> splitString(std::string &str, const char delimiter)
{
    trim(str);
    if (str == "") return std::vector<std::string>{};

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

constexpr double ln(const double x) {
    assert(x > 0);
    return log(x);
}

inline auto millisecondsElapsed(const std::chrono::steady_clock::time_point start)
{
    return (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
}

u64 gRngState = 1070372;

inline void resetRng() { 
    gRngState = 1070372;
}

inline u64 randomU64() {
    gRngState ^= gRngState >> 12;
    gRngState ^= gRngState << 25;
    gRngState ^= gRngState >> 27;
    return gRngState * 2685821657736338717LL;
}

// [color][CASTLE_SHORT or CASTLE_LONG or isLongCastle]
constexpr MultiArray<u64, 2, 2> CASTLING_MASKS = {{
    // White short and long castle
    { bitboard(7), bitboard(0) },
    // Black short and long castle
    { bitboard(63), bitboard(56) } 
}};

// [kingTargetSquare]
std::array<std::pair<Square, Square>, 64> CASTLING_ROOK_FROM_TO = {};

// [from][to] (BETWEEN excludes from and to)
MultiArray<u64, 64, 64> BETWEEN = {}, LINE_THROUGH = {};

#include "attacks.hpp"

constexpr void initUtils() {
    attacks::init();

    CASTLING_ROOK_FROM_TO[6] = {7, 5};    // White short castle
    CASTLING_ROOK_FROM_TO[2] = {0, 3};    // White long castle
    CASTLING_ROOK_FROM_TO[62] = {63, 61}; // Black short castle
    CASTLING_ROOK_FROM_TO[58] = {56, 59}; // Black long castle

    for (Square sq1 = 0; sq1 < 64; sq1++) {
        for (Square sq2 = 0; sq2 < 64; sq2++) 
        {
            if (sq1 == sq2) continue;

            LINE_THROUGH[sq1][sq2] = bitboard(sq1) | bitboard(sq2);

            if (attacks::bishopAttacks(sq1, 0) & bitboard(sq2)) 
            {
                BETWEEN[sq1][sq2] = attacks::bishopAttacks(sq1, bitboard(sq2)) & attacks::bishopAttacks(sq2, bitboard(sq1));
                LINE_THROUGH[sq1][sq2] |= attacks::bishopAttacks(sq1, 0) & attacks::bishopAttacks(sq2, 0);
            }
            else  if (attacks::rookAttacks(sq1, 0) & bitboard(sq2)) 
            {
                BETWEEN[sq1][sq2] = attacks::rookAttacks(sq1, bitboard(sq2)) & attacks::rookAttacks(sq2, bitboard(sq1));
                LINE_THROUGH[sq1][sq2] |= attacks::rookAttacks(sq1, 0) & attacks::rookAttacks(sq2, 0);
            }

      }
    }
}
