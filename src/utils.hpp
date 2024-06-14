// clang-format off

#pragma once

#define stringify(myVar) (std::string)#myVar

#include <array>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <unordered_map>
#include "3rdparty/ordered_map.h"
#include "types.hpp"
#include "array_extensions.hpp"

#if defined(__GNUC__) // GCC, Clang, ICC

    inline u8 lsb(u64 b)
    {
        assert(b);
        return u8(__builtin_ctzll(b));
    }
    inline u8 msb(u64 b)
    {
        assert(b);
        return u8(63 ^ __builtin_clzll(b));
    }

#else // Assume MSVC Windows 64

    #include <intrin.h>
    inline u8 lsb(u64 b)
    {
        unsigned long idx;
        _BitScanForward64(&idx, b);
        return (u8)idx;
    }
    inline u8 msb(u64 b)
    {
        unsigned long idx;
        _BitScanReverse64(&idx, b);
        return (u8)idx;
    }

#endif

inline u8 poplsb(u64 &mask)
{
    u8 s = lsb(mask);
    mask &= mask - 1; // compiler optimizes this to _blsr_u64
    return u8(s);
}

inline u64 pdep(u64 val, u64 mask) {
    u64 res = 0;
    for (u64 bb = 1; mask; bb += bb) {
        if (val & bb)
            res |= mask & -mask;
        mask &= mask - 1;
    }
    return res;
}

inline void trim(std::string &str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    
    str = first == std::string::npos 
          ? "" 
          : str.substr(first, (last - first + 1));
}

inline std::vector<std::string> splitString(std::string &str, char delimiter)
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

inline u64 toBitboard(Square sq) { return 1ULL << sq; }

inline void printBitboard(u64 bb)
{
    std::bitset<64> b(bb);
    std::string str_bitset = b.to_string(); 
    for (int i = 0; i < 64; i += 8)
    {
        std::string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());
        for (u64 j = 0; j < x.length(); j++)
            std::cout << std::string(1, x[j]) << " ";
        std::cout << std::endl;
    }
}

inline int charToInt(char myChar) { return myChar - '0'; }

inline double ln(u64 x) {
    assert(x > 0);
    return log(x);
}

inline u64 shiftRight(u64 bb) {
	return (bb << 1ULL) & 0xfefefefefefefefeULL;
}

inline u64 shiftLeft(u64 bb) {
	return (bb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL;
}

inline u64 shiftUp(u64 bb) { return bb << 8ULL; }

inline u64 shiftDown(u64 bb) { return bb >> 8ULL; }

inline Color oppColor(Color color)
{
    assert(color != Color::NONE);
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

inline Rank squareRank(Square square) { return (Rank)(square / 8); }

inline File squareFile(Square square) { return (File)(square % 8); }

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

inline Square strToSquare(std::string strSquare) {
    return (strSquare[0] - 'a') + (strSquare[1] - '1') * 8;
}

std::unordered_map<char, Piece> CHAR_TO_PIECE = {
    {'P', Piece::WHITE_PAWN},
    {'N', Piece::WHITE_KNIGHT},
    {'B', Piece::WHITE_BISHOP},
    {'R', Piece::WHITE_ROOK},
    {'Q', Piece::WHITE_QUEEN},
    {'K', Piece::WHITE_KING},
    {'p', Piece::BLACK_PAWN},
    {'n', Piece::BLACK_KNIGHT},
    {'b', Piece::BLACK_BISHOP},
    {'r', Piece::BLACK_ROOK},
    {'q', Piece::BLACK_QUEEN},
    {'k', Piece::BLACK_KING},
};

std::unordered_map<char, PieceType> CHAR_TO_PIECE_TYPE = {
    {'P', PieceType::PAWN},
    {'N', PieceType::KNIGHT},
    {'B', PieceType::BISHOP},
    {'R', PieceType::ROOK},
    {'Q', PieceType::QUEEN},
    {'K', PieceType::KING},
    {'p', PieceType::PAWN},
    {'n', PieceType::KNIGHT},
    {'b', PieceType::BISHOP},
    {'r', PieceType::ROOK},
    {'q', PieceType::QUEEN},
    {'k', PieceType::KING},
};

std::unordered_map<Piece, char> PIECE_TO_CHAR = {
    {Piece::WHITE_PAWN,   'P'},
    {Piece::WHITE_KNIGHT, 'N'},
    {Piece::WHITE_BISHOP, 'B'},
    {Piece::WHITE_ROOK,   'R'},
    {Piece::WHITE_QUEEN,  'Q'},
    {Piece::WHITE_KING,   'K'},
    {Piece::BLACK_PAWN,   'p'},
    {Piece::BLACK_KNIGHT, 'n'},
    {Piece::BLACK_BISHOP, 'b'},
    {Piece::BLACK_ROOK,   'r'},
    {Piece::BLACK_QUEEN,  'q'},
    {Piece::BLACK_KING,   'k'}
};

inline PieceType pieceToPieceType(Piece piece)
{
    if (piece == Piece::NONE) return PieceType::NONE;

    int intPiece = (int)piece;
    return intPiece <= 5 ? (PieceType)intPiece : (PieceType)(intPiece - 6);
}

inline Color pieceColor(Piece piece)
{
    return (int)piece <= 5 ? Color::WHITE
           : (int)piece <= 11 ? Color::BLACK
           : Color::NONE;
}

inline Piece makePiece(PieceType pieceType, Color color)
{
    assert(pieceType != PieceType::NONE);
    int pt = (int)pieceType;
    return color == Color::WHITE ? (Piece)pt : (Piece)(pt+6);
}

inline auto millisecondsElapsed(std::chrono::steady_clock::time_point start)
{
    return (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
}

// [color][CASTLE_SHORT or CASTLE_LONG or isLongCastle]
constexpr MultiArray<u64, 2, 2> CASTLING_MASKS = {{
    // White short and long castle
    { 1ULL << 7, 1ULL },
    // Black short and long castle
    { 1ULL << 63, 1ULL << 56 } 
}};

std::array<std::pair<Square, Square>, 64> CASTLING_ROOK_FROM_TO = {};

// [from][to] (BETWEEN excludes from and to)
MultiArray<u64, 64, 64> BETWEEN = {}, LINE_THROUGH = {};

#include "attacks.hpp"

constexpr void initUtils() {
    CASTLING_ROOK_FROM_TO[6] = {7, 5};    // White short castle
    CASTLING_ROOK_FROM_TO[2] = {0, 3};    // White long castle
    CASTLING_ROOK_FROM_TO[62] = {63, 61}; // Black short castle
    CASTLING_ROOK_FROM_TO[58] = {56, 59}; // Black long castle

    for (Square sq1 = 0; sq1 < 64; sq1++) {
        for (Square sq2 = 0; sq2 < 64; sq2++) 
        {
            if (sq1 == sq2) continue;

            u64 bbSq1 = 1ULL << sq1;
            u64 bbSq2 = 1ULL << sq2;
            LINE_THROUGH[sq1][sq2] = bbSq1 | bbSq2;

            if (attacks::bishopAttacks(sq1, 0) & bbSq2) 
            {
                BETWEEN[sq1][sq2] = attacks::bishopAttacks(sq1, bbSq2) & attacks::bishopAttacks(sq2, bbSq1);
                LINE_THROUGH[sq1][sq2] |= attacks::bishopAttacks(sq1, 0) & attacks::bishopAttacks(sq2, 0);
            }
            else  if (attacks::rookAttacks(sq1, 0) & bbSq2) 
            {
                BETWEEN[sq1][sq2] = attacks::rookAttacks(sq1, bbSq2) & attacks::rookAttacks(sq2, bbSq1);
                LINE_THROUGH[sq1][sq2] |= attacks::rookAttacks(sq1, 0) & attacks::rookAttacks(sq2, 0);
            }

      }
    }
    
}
