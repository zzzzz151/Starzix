#pragma once

#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <bitset>
#include "types.hpp"
using namespace std;

inline uint8_t squareFile(Square square) { return square & 7; }

inline uint8_t squareRank(Square square) { return square >> 3; }

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
        return  NULL_COLOR;
}

inline Piece makePiece(PieceType pieceType, Color color)
{
    if (color == NULL_COLOR || pieceType == PieceType::NONE)
        return Piece::NONE;
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
    if (color == NULL_COLOR) 
        return NULL_COLOR;
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

template <typename T>
inline bool lastElementsAreEqual(vector<T> &moves, int numElements) {
    if (numElements > moves.size()) 
        return false;

    T lastElement = moves[moves.size() - 1];

    // Compare last numElements to lastElement
    for (int i = 2; i <= numElements; i++) 
        if (moves[moves.size() - i] != lastElement) 
            return false;

    return true;
}