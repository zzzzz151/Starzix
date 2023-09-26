#ifndef MOVE_HPP
#define MOVE_HPP

// clang-format off
#include <cassert>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
using namespace std;

struct Move
{
    private:

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)  
    uint16_t moveEncoded = 0;

    public:

    const static uint16_t NULL_FLAG = 0x0000,
                          NORMAL_FLAG = 0x0001,
                          CASTLING_FLAG = 0x0002,
                          EN_PASSANT_FLAG = 0x0003,
                          KNIGHT_PROMOTION_FLAG = 0x0004,
                          BISHOP_PROMOTION_FLAG = 0x0005,
                          ROOK_PROMOTION_FLAG = 0x0006,
                          QUEEN_PROMOTION_FLAG = 0x0007;

    constexpr static uint16_t PROMOTION_FLAGS[4] = {QUEEN_PROMOTION_FLAG, KNIGHT_PROMOTION_FLAG, BISHOP_PROMOTION_FLAG, ROOK_PROMOTION_FLAG};

    Move() = default;

    // Custom == operator
    bool operator==(const Move& other) const {
        return moveEncoded == other.moveEncoded;
    }

    // Custom != operator
    bool operator!=(const Move& other) const {
        return moveEncoded != other.moveEncoded;
    }

    inline Move(Square from, Square to, uint16_t typeFlag)
    {
        // from/to: 00100101
        // (uint16_t)from/to: 00000000 00100101

        moveEncoded = ((uint16_t)from << 10);
        moveEncoded |= ((uint16_t)to << 4);
        moveEncoded |= typeFlag;
    }

    inline Move(string from, string to, uint16_t typeFlag) : Move(strToSquare(from), strToSquare(to), typeFlag) {}

    static inline Move fromUci(string uci, Piece* boardPieces)
    {
        Square from = strToSquare(uci.substr(0,2));
        Square to = strToSquare(uci.substr(2,4));

        if (uci.size() == 5) // promotion
        {
            char promotionLowerCase = uci.back(); // last char of string
            uint16_t typeFlag = QUEEN_PROMOTION_FLAG;

            if (promotionLowerCase == 'n') 
                typeFlag = KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                typeFlag = BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                typeFlag = ROOK_PROMOTION_FLAG;

            return Move(from, to, typeFlag);
        }

        PieceType pieceType = pieceTypeAt(from, boardPieces);
        if (pieceType == PieceType::PAWN && boardPieces[to] == Piece::NONE && squareFile(from) != squareFile(to))
            return Move(from, to, EN_PASSANT_FLAG);

        if (pieceType == PieceType::KING)
        {
            int bitboardSquaresMoved = (int)to - (int)from;
            if (bitboardSquaresMoved == 2 || bitboardSquaresMoved == -2)
                return Move(from, to, CASTLING_FLAG);
        }

        return Move(from, to, NORMAL_FLAG);
    }

    inline Square from() { return (moveEncoded >> 10) & 0b111111; }

    inline Square to() { return (moveEncoded >> 4) & 0b111111; }

    inline uint16_t typeFlag() { return moveEncoded & 0x000F; }

    inline PieceType promotionPieceType()
    {
        uint16_t flag = typeFlag();
        if (flag == QUEEN_PROMOTION_FLAG) return PieceType::QUEEN;
        if (flag == KNIGHT_PROMOTION_FLAG) return PieceType::KNIGHT;
        if (flag == BISHOP_PROMOTION_FLAG) return PieceType::BISHOP;
        if (flag == ROOK_PROMOTION_FLAG) return PieceType::ROOK;
        return PieceType::NONE;
    }

    inline string toUci()
    {
        string str = squareToStr[from()] + squareToStr[to()];
        uint16_t myTypeFlag = typeFlag();
        if (myTypeFlag == QUEEN_PROMOTION_FLAG) str += "q";
        else if (myTypeFlag == KNIGHT_PROMOTION_FLAG) str += "n";
        else if (myTypeFlag == BISHOP_PROMOTION_FLAG) str += "b";
        else if (myTypeFlag == ROOK_PROMOTION_FLAG) str += "r";
        return str;
    }
    
};

struct MovesList
{
    private:

    Move moves[255];
    uint8_t numMoves = 0;

    public:

    inline void add(Move move) { 
        assert(numMoves < 255);
        moves[numMoves++] = move; 
    }

    inline uint8_t size() { return numMoves; }

    inline Move operator[](int i) {
        assert(i >= 0 && i < numMoves);
        return moves[i];
    }

    inline void swap(int i, int j)
    {
    	assert(i >= 0 && j >= 0 && i < numMoves && j < numMoves);
        Move temp = moves[i];
        moves[i] = moves[j];
        moves[j] = temp;
    }
};

#endif