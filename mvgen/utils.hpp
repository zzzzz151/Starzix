#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "types.hpp"
using namespace std;

uint64_t knightMoves[64], kingMoves[64];

inline vector<string> splitString(string str, char delimiter)
{
    vector<string> strSplit;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter))
        strSplit.push_back(token);

    return strSplit;
}

inline string trim(string &str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    
    if (first == std::string::npos) // The string is empty or contains only whitespace characters
        return "";
    
    return str.substr(first, (last - first + 1));
}

inline void printBitboard(uint64_t bb)
{
    std::bitset<64> b(bb);
    std::string str_bitset = b.to_string();
    for (int i = 0; i < 64; i += 8)
    {
        std::string x = str_bitset.substr(i, 8);
        reverse(x.begin(), x.end());
        std::cout << x << std::endl;
    }
}

inline int charToInt(char myChar)
{
    return myChar - '0';
}

inline PieceType pieceToPieceType(Piece piece)
{
    if (piece == Piece::NONE) return PieceType::NONE;
    return (PieceType)((uint8_t)piece % 6);
}

inline Color pieceColor(Piece piece)
{
    if ((uint8_t)piece <= 5) return WHITE;
    else if ((uint8_t)piece <= 11) return BLACK;
    else return NULL_COLOR;
}

inline Square strToSquare(string strSquare)
{
    return (strSquare[0] - 'a') + (strSquare[1] - '1') * 8;
}

inline char squareFile(Square square)
{
    int file = square & 7;
    switch (file)
    {
        case 0:
            return 'a';
        case 1:
            return 'b';
        case 2:
            return 'c';
        case 3:
            return 'd';
        case 4:
            return 'e';
        case 5:
            return 'f';
        case 6:
            return 'g';
        case 7:
            return 'h';
        default:
            return 'z';
    }
}

inline uint8_t squareRank(Square square)
{
    return square >> 3;
}

inline bool squareIsBackRank(Square square)
{
    uint8_t rank = squareRank(square);
    return rank == 0 || rank == 7;
}

inline bool isEdgeSquare(Square square)
{
    char file = squareFile(square);
    return file == 'a' || file == 'h' || squareIsBackRank(square);
}

inline PieceType pieceTypeAt(Square sq, Piece* boardPieces)
{
    return pieceToPieceType(boardPieces[sq]);
}

inline void initKnightMoves(Square square)
{
    uint64_t n = 1ULL << square;
    uint64_t h1 = ((n >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL) | ((n << 1ULL) & 0xfefefefefefefefeULL);
    uint64_t h2 = ((n >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL) | ((n << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);
    knightMoves[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
}

inline void initKingMoves(Square square)
{
    char file = squareFile(square);
    uint64_t k = 1ULL << square;
    k |= (k << 8ULL) | (k >> 8ULL);
    k |= ((k & (file != 'a')) >> 1ULL) | ((k & (file != 'h')) << 1ULL);
    kingMoves[square] = k ^ (1ULL << square);
}

inline void initMoves()
{
    for (int square = 0; square < 64; square++)
    {
        initKnightMoves(square);
        initKingMoves(square);
    }
}

inline void getDirOffsetsIndexes(Square square, int &dirOffsetsStartIndex, int &dirOffsetsEndIndex)
{
    dirOffsetsStartIndex = 0;
    dirOffsetsEndIndex = 7;
    uint8_t rank = squareRank(square);
    char file = squareFile(square);

    if (square == 56) // a8 (top left)
    {
        dirOffsetsStartIndex = RIGHT;
        dirOffsetsEndIndex = DOWN;
    }
    else if (square == 63) // h8 (top right)
    {
        dirOffsetsStartIndex = DOWN;
        dirOffsetsEndIndex = LEFT;
    }
    else if (square == 0) // a1 (bottom left)
    {
        dirOffsetsStartIndex = UP;
        dirOffsetsEndIndex = RIGHT;
    }
    else if (square == 7) // h1 (bottom right)
    {
        dirOffsetsStartIndex = LEFT;
        dirOffsetsEndIndex = UP;
    }
    else if (rank == 0)
    {
        dirOffsetsStartIndex = LEFT;
        dirOffsetsEndIndex = RIGHT;
    }
    else if (rank == 7)
    {
        dirOffsetsStartIndex = RIGHT;
        dirOffsetsEndIndex = LEFT;
    }
    else if (file == 'a')
    {
        dirOffsetsStartIndex = UP;
        dirOffsetsEndIndex = DOWN; 
    }
    else if (file == 'h')
    {
        dirOffsetsStartIndex = DOWN;
        dirOffsetsEndIndex = UP; 
    }
}




#endif