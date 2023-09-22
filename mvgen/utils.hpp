#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>
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

inline int charToInt(char myChar)
{
    return myChar - '0';
}

inline PieceType pieceToPieceType(Piece piece)
{
    if (piece == Piece::NONE) return PieceType::NONE;
    return (PieceType)((uint8_t)piece % 6);
}

inline char pieceColor(Piece piece)
{
    if ((uint8_t)piece <= 5) return WHITE;
    else if ((uint8_t)piece <= 11) return BLACK;
    else return NULL_COLOR;
}

inline uint8_t stringToSquare(string square)
{
    return (square[0] - 'a') + (square[1] - '1') * 8;
}

inline char squareFile(uint8_t square)
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

inline uint8_t squareRank(uint8_t square)
{
    return square >> 3;
}

#endif