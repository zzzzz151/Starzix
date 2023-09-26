#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "types.hpp"
using namespace std;

uint64_t knightMoves[64], kingMoves[64];

inline void initKnightMoves(Square square)
{
    uint64_t n = 1ULL << square;
    uint64_t h1 = ((n >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL) | ((n << 1ULL) & 0xfefefefefefefefeULL);
    uint64_t h2 = ((n >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL) | ((n << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);
    knightMoves[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
}

inline void initKingMoves(Square square)
{
    uint64_t fileA = 0x8080808080808080;
    uint64_t fileH = 0x0101010101010101;

    uint64_t k = 1ULL << square;
    k |= (k << 8ULL) | (k >> 8ULL);
    k |= ((k & fileA) >> 1ULL) | ((k & fileH) << 1ULL);
    kingMoves[square] = k ^ (1ULL << square);
}

inline uint64_t shiftRight(uint64_t bb) {
	return (bb << 1ULL) & 0xfefefefefefefefeULL;
}

inline uint64_t shiftLeft(uint64_t bb) {
	return (bb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL;
}

inline uint64_t shiftUp(uint64_t bb) { return bb << 8ULL; }

inline uint64_t shiftDown(uint64_t bb) { return bb >> 8ULL; }

inline void initMoves()
{
    uint64_t king = 1;

    for (int square = 0; square < 64; square++)
    {
        initKnightMoves(square);
        
        uint64_t attacks = shiftLeft(king) | shiftRight(king) | king;
        attacks = (attacks | shiftUp(attacks) | shiftDown(attacks)) ^ king;
        kingMoves[square] = attacks;
        king <<= 1ULL;
    }
}

inline char squareFile(Square square)
{
    int file = square & 7;
    switch (file)
    {
        case 0: return 'a';
        case 1: return 'b';
        case 2: return 'c';
        case 3: return 'd';
        case 4: return 'e';
        case 5: return 'f';
        case 6: return 'g';
        case 7: return 'h';
        default: return 'z';
    }
}

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
        cout << x << endl;
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

inline bool isEdgeSquare(Square square)
{
    char file = squareFile(square);
    return file == 'a' || file == 'h' || squareIsBackRank(square);
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

#endif