#pragma once

#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <cassert>
#include "types.hpp"
using namespace std;

inline uint8_t lsb(uint64_t b);

inline uint8_t msb(uint64_t b);

#if defined(__GNUC__) // GCC, Clang, ICC
inline uint8_t lsb(uint64_t b)
{
    assert(b);
    return uint8_t(__builtin_ctzll(b));
}

inline uint8_t msb(uint64_t b)
{
    assert(b);
    return uint8_t(63 ^ __builtin_clzll(b));
}

#elif defined(_MSC_VER) // MSVC

#ifdef _WIN64 // MSVC, WIN64
#include <intrin.h>
inline uint8_t lsb(uint64_t b)
{
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return (uint8_t)idx;
}

inline uint8_t msb(uint64_t b)
{
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return (uint8_t)idx;
}

#else // MSVC, WIN32
#include <intrin.h>
inline uint8_t lsb(uint64_t b)
{
    unsigned long idx;

    if (b & 0xffffffff)
    {
        _BitScanForward(&idx, int32_t(b));
        return uint8_t(idx);
    }
    else
    {
        _BitScanForward(&idx, int32_t(b >> 32));
        return uint8_t(idx + 32);
    }
}

inline uint8_t msb(uint64_t b)
{
    unsigned long idx;

    if (b >> 32)
    {
        _BitScanReverse(&idx, int32_t(b >> 32));
        return uint8_t(idx + 32);
    }
    else
    {
        _BitScanReverse(&idx, int32_t(b));
        return uint8_t(idx);
    }
}

#endif

#else // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

inline uint8_t poplsb(uint64_t &mask)
{
    uint8_t s = lsb(mask);
    mask &= mask - 1; // compiler optimizes this to _blsr_uint64_t
    return uint8_t(s);
}

inline uint64_t pdep(uint64_t val, uint64_t mask) {
    uint64_t res = 0;
    for (uint64_t bb = 1; mask; bb += bb) {
        if (val & bb)
            res |= mask & -mask;
        mask &= mask - 1;
    }
    return res;
}

inline uint8_t squareFile(Square square) { return square & 7; }

inline uint8_t squareRank(Square square) { return square >> 3; }

inline void trim(string &str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    
    if (first == string::npos) // The string is empty or contains only whitespace characters
    {
        str = "";
        return;
    }
    
    str = str.substr(first, (last - first + 1));
}

inline vector<string> splitString(string &str, char delimiter)
{
    trim(str);
    if (str == "")
        return vector<string>{};

    vector<string> strSplit;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter))
        strSplit.push_back(token);

    return strSplit;
}


inline void printBitboard(uint64_t bb)
{
    bitset<64> b(bb);
    string str_bitset = b.to_string();
    for (int i = 0; i < 64; i += 8)
    {
        string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());
        for (int j = 0; j < x.length(); j++)
            cout << string(1, x[j]) << " ";
        cout << endl;
    }
}

inline int charToInt(char myChar) { return myChar - '0'; }

inline PieceType pieceToPieceType(Piece piece)
{
    if (piece == Piece::NONE) return PieceType::NONE;
    return (PieceType)((uint8_t)piece % 6);
}

inline Color pieceColor(Piece piece)
{
    if ((uint8_t)piece <= 5) 
        return WHITE;
    else if ((uint8_t)piece <= 11) 
        return BLACK;
    else 
        return NULL_COLOR;
}

inline Piece makePiece(PieceType pieceType, Color color)
{
    int piece = (int)pieceType;
    if (color == BLACK) 
        piece += 6;
    return (Piece)piece;
}

inline Square strToSquare(string strSquare)
{
    return (strSquare[0] - 'a') + (strSquare[1] - '1') * 8;
}

inline bool squareIsBackRank(Square square)
{
    uint8_t rank = squareRank(square);
    return rank == 0 || rank == 7;
}

inline PieceType pieceTypeAt(Square sq, Piece* boardPieces)
{
    return pieceToPieceType(boardPieces[sq]);
}

inline Color oppColor(Color color)
{
    return color == WHITE ? BLACK : WHITE;
}

inline uint64_t shiftRight(uint64_t bb) {
	return (bb << 1ULL) & 0xfefefefefefefefeULL;
}

inline uint64_t shiftLeft(uint64_t bb) {
	return (bb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL;
}

inline uint64_t shiftUp(uint64_t bb) { return bb << 8ULL; }

inline uint64_t shiftDown(uint64_t bb) { return bb >> 8ULL; }

inline double ln(int x)
{
    assert(x > 0);
    return log(x);
}

inline int16_t min(int16_t a, int16_t b)
{
    return a < b ? a : b;
}

inline int16_t max(int16_t a, int16_t b)
{
    return a > b ? a : b;
}

#include "move.hpp"

inline pair<Move, int32_t> incrementalSort(MovesList &moves, array<int32_t, 256> &movesScores, int i)
{
    for (int j = i + 1; j < moves.size(); j++)
        if (movesScores[j] > movesScores[i])
        {
            moves.swap(i, j);
            swap(movesScores[i], movesScores[j]);
        }

    return { moves[i], movesScores[i] };
}