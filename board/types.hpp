#ifndef TYPES_HPP
#define TYPES_HPP

// clang-format off
#include <cstdint>
#include <string>
#include <unordered_map>
using namespace std;

const string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

using Square = uint8_t;
using Color = char;

const string squareToStr[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

const Color WHITE = 0, BLACK = 1, NULL_COLOR = 2;

enum class PieceType : uint8_t
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE
};

enum class Piece : uint8_t
{
    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,
    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING,
    NONE
};

unordered_map<char, Piece> charToPiece = {
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

unordered_map<Piece, char> pieceToChar = {
    {Piece::WHITE_PAWN, 'P'},
    {Piece::WHITE_KNIGHT, 'N'},
    {Piece::WHITE_BISHOP, 'B'},
    {Piece::WHITE_ROOK, 'R'},
    {Piece::WHITE_QUEEN, 'Q'},
    {Piece::WHITE_KING, 'K'},
    {Piece::BLACK_PAWN, 'p'},
    {Piece::BLACK_KNIGHT, 'n'},
    {Piece::BLACK_BISHOP, 'b'},
    {Piece::BLACK_ROOK, 'r'},
    {Piece::BLACK_QUEEN, 'q'},
    {Piece::BLACK_KING, 'k'}
};

const int LEFT = 0, UP = 1, RIGHT = 2, DOWN = 3, UP_LEFT = 4, UP_RIGHT = 5, DOWN_RIGHT = 6, DOWN_LEFT = 7;
const int directionsOffsets[8] = {-1, 8, 1, -8, 7, 9, -7, -9};
const bool directionWillHitFileA[8] = {true, false, false, false, true, false, false, true},
           directionWillHitFileH[8] = {false, false, true, false, false, true, true, false},
           directionWillHitRank0[8] = {false, false, false, true, false, false, true, true},
           directionWillHitRank7[8] = {false, true, false, false, true, true, false, false};

#endif