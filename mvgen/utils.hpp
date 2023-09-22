#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "types.hpp"
using namespace std;

inline vector<string> splitString(string str, char delimiter)
{
    vector<string> strSplit;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter))
        strSplit.push_back(token);

    return strSplit;
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

inline bool isBackRank(uint8_t rank)
{
    return rank == 0 || rank == 7;
}

#endif