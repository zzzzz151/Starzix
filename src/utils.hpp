#pragma once

#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <unordered_map>
#include "types.hpp"
using namespace std;

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

const string SQUARE_TO_STR[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

unordered_map<char, Piece> CHAR_TO_PIECE = {
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

unordered_map<Piece, char> PIECE_TO_CHAR = {
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

inline Rank squareRank(Square square) { return (Rank)(square / 8); }

inline File squareFile(Square square) { return (File)(square % 8); }

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


inline void printBitboard(u64 bb)
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
    return piece == Piece::NONE ? PieceType::NONE
                                : (PieceType)((u8)piece % 6);
}

inline Color pieceColor(Piece piece)
{
    if ((u8)piece <= 5) return Color::WHITE;

    if ((u8)piece <= 11) return Color::BLACK;

    return Color::NONE;
}

inline Piece makePiece(PieceType pieceType, Color color)
{
    int piece = (int)pieceType;
    if (color == Color::BLACK) piece += 6;

    return (Piece)piece;
}

inline Square strToSquare(string strSquare) {
    return (strSquare[0] - 'a') + (strSquare[1] - '1') * 8;
}

inline Color oppColor(Color color)
{
    assert(color != Color::NONE);
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

inline u64 shiftRight(u64 bb) {
	return (bb << 1ULL) & 0xfefefefefefefefeULL;
}

inline u64 shiftLeft(u64 bb) {
	return (bb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL;
}

inline u64 shiftUp(u64 bb) { return bb << 8ULL; }

inline u64 shiftDown(u64 bb) { return bb >> 8ULL; }

inline double ln(int x)
{
    assert(x > 0);
    return log(x);
}

inline i16 min(i16 a, i16 b) {
    return a < b ? a : b;
}

inline i16 max(i16 a, i16 b) {
    return a > b ? a : b;
}

inline auto millisecondsElapsed(chrono::steady_clock::time_point start)
{
    return (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
}

#include "move.hpp"

inline pair<Move, i32> incrementalSort(MovesList &moves, array<i32, 256> &movesScores, int i)
{
    for (int j = i + 1; j < moves.size(); j++)
        if (movesScores[j] > movesScores[i])
        {
            moves.swap(i, j);
            swap(movesScores[i], movesScores[j]);
        }

    return { moves[i], movesScores[i] };
}