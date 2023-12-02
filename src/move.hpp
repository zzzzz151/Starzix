#pragma once

// clang-format off
#include <cassert>
#include "types.hpp"
#include "utils.hpp"

struct Move
{
    private:

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)  
    u16 moveEncoded = 0;

    public:

    const static u16 NULL_FLAG = 0x0000,
                     NORMAL_FLAG = 0x0005,
                     CASTLING_FLAG = 0x0006,
                     EN_PASSANT_FLAG = 0x0007,
                     PAWN_TWO_UP_FLAG = 0x0008,
                     KNIGHT_PROMOTION_FLAG = 0x0001,
                     BISHOP_PROMOTION_FLAG = 0x0002,
                     ROOK_PROMOTION_FLAG = 0x0003,
                     QUEEN_PROMOTION_FLAG = 0x0004;

    inline Move() = default;

    inline bool operator==(const Move &other) const {
        return moveEncoded == other.moveEncoded;
    }

    inline bool operator!=(const Move &other) const {
        return moveEncoded != other.moveEncoded;
    }

    inline Move(Square from, Square to, u16 typeFlag)
    {
        // from/to: 00100101
        // (u16)from/to: 00000000 00100101

        moveEncoded = ((u16)from << 10);
        moveEncoded |= ((u16)to << 4);
        moveEncoded |= typeFlag;
    }

    inline u16 getMoveEncoded() { return moveEncoded; }

    inline Square from() { return (moveEncoded >> 10) & 0b111111; }

    inline Square to() { return (moveEncoded >> 4) & 0b111111; }

    inline u16 typeFlag() { return moveEncoded & 0x000F; }

    inline PieceType promotion()
    {
        u16 flag = typeFlag();
        if (flag < 1 || flag > 4)
            return PieceType::NONE;
        return (PieceType)flag;
    }

    static inline Move fromUci(std::string uci, std::array<Piece, 64> pieces)
    {
        Square from = strToSquare(uci.substr(0,2));
        Square to = strToSquare(uci.substr(2,4));
        PieceType pieceType = pieceToPieceType(pieces[from]);

        if (uci.size() == 5) // promotion
        {
            char promotionLowerCase = uci.back(); // last char of string
            u16 typeFlag = QUEEN_PROMOTION_FLAG;

            if (promotionLowerCase == 'n') 
                typeFlag = KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                typeFlag = BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                typeFlag = ROOK_PROMOTION_FLAG;

            return Move(from, to, typeFlag);
        }
        else if (pieceType == PieceType::KING)
        {
            if (abs((int)to - (int)from) == 2)
                return Move(from, to, Move::CASTLING_FLAG);
        }
        else if (pieceType == PieceType::PAWN)
        { 
            int bitboardSquaresTraveled = abs((int)to - (int)from);
            if (bitboardSquaresTraveled == 16)
                return Move(from, to, PAWN_TWO_UP_FLAG);
            if (bitboardSquaresTraveled != 8 && pieces[to] == Piece::NONE)
                return Move(from, to, EN_PASSANT_FLAG);
        }

        return Move(from, to, NORMAL_FLAG);
    }

    inline std::string toUci()
    {
        std::string str = SQUARE_TO_STR[from()] + SQUARE_TO_STR[to()];
        u16 myTypeFlag = typeFlag();

        if (myTypeFlag == QUEEN_PROMOTION_FLAG) 
            str += "q";
        else if (myTypeFlag == KNIGHT_PROMOTION_FLAG) 
            str += "n";
        else if (myTypeFlag == BISHOP_PROMOTION_FLAG) 
            str += "b";
        else if (myTypeFlag == ROOK_PROMOTION_FLAG) 
            str += "r";

        return str;
    }
    
};

Move MOVE_NONE = Move();

struct MovesList
{
    private:

    Move moves[256];
    u8 numMoves = 0;

    public:

    inline MovesList() = default;

    inline void add(Move move) { 
        assert(numMoves < 255);
        moves[numMoves++] = move; 
    }

    inline u8 size() { return numMoves; }

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

    inline void shuffle()
    {
        if (numMoves <= 1) return;

        // Use the current time as a seed
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, numMoves-1); // both inclusive

        // Fisher-Yates shuffle algorithm 
        for (int i = 0; i < numMoves; i++)
        {
            int randomIndex = dis(gen);
            swap(i, randomIndex);
        }
    }
};

