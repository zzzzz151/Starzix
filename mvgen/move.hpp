#ifndef MOVE_HPP
#define MOVE_HPP

// clang-format off
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"

struct Move
{
    public:

    const static uint16_t NORMAL_FLAG = 0x0000,
                          CASTLING_FLAG = 0x0001,
                          EN_PASSANT_FLAG = 0x0002,
                          KNIGHT_PROMOTION_FLAG = 0x0003,
                          BISHOP_PROMOTION_FLAG = 0x0004,
                          ROOK_PROMOTION_FLAG = 0x0005,
                          QUEEN_PROMOTION_FLAG = 0x0006;

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)  
    uint16_t move;

    inline Move(Square from, Square to, PieceType pieceType, PieceType promotionPieceType = PieceType::QUEEN)
    {
        // from/to: 00100101
        // (uint16_t)from/to: 00000000 00100101
        cout << "uint8_t from arg received: " << (int)from << endl;
        move = ((uint16_t)from << 10);
        cout << "in constructor!! " << (uint16_t)(this->from()) << endl;
        move |= ((uint16_t)to << 4);
        if (pieceType == PieceType::PAWN)
        {
            uint8_t rank = squareRank(to);
            if (isBackRank(to)) move |= promotionPieceTypeToFlag(promotionPieceType);
        }
        else if (pieceType == PieceType::KING)
        {
            // white castle
            if (from == 4) if (to == 0 || to == 7) move |= CASTLING_FLAG;
            // black castle
            if (from == 60) if (to == 56 || to == 63) move |= CASTLING_FLAG;
        }
    }

    inline Move(string from, string to, PieceType pieceType)
    {
        Move(strToSquare(from), strToSquare(to), pieceType);
    }

    inline Move(string uci, PieceType pieceType)
    {
        Square from = strToSquare(uci.substr(0,2));
        Square to = strToSquare(uci.substr(2,4));

        if (uci.size() == 5) // promotion
        {
            char promotionLowerCase = uci.back(); // last char of string
            if (promotionLowerCase == 'n') Move(from, to, pieceType, PieceType::KNIGHT);
            else if (promotionLowerCase == 'b') Move(from, to, pieceType, PieceType::BISHOP);
            else if (promotionLowerCase == 'r') Move(from, to, pieceType, PieceType::ROOK);
            else if (promotionLowerCase == 'q') Move(from, to, pieceType, PieceType::QUEEN);
            else Move(from, to, pieceType);
        }
        else
            Move(from, to, pieceType);
    }

    inline Square from()
    {
        return (move >> 10) & 0xff;
    }

    inline Square to()
    {
        return move & 0b000000'111111'0000;
    }

    inline uint16_t getTypeFlag()
    {
        return move & 0x000F;
    }

    inline PieceType flagToPieceType(uint8_t promotionFlag)
    {
        if (promotionFlag == KNIGHT_PROMOTION_FLAG) return PieceType::KNIGHT;
        if (promotionFlag == BISHOP_PROMOTION_FLAG) return PieceType::BISHOP;
        if (promotionFlag == ROOK_PROMOTION_FLAG) return PieceType::ROOK;
        if (promotionFlag == QUEEN_PROMOTION_FLAG) return PieceType::QUEEN;
        return PieceType::NONE;
    }

    inline uint16_t promotionPieceTypeToFlag(PieceType promotionPT)
    {
        if (promotionPT == PieceType::QUEEN) return QUEEN_PROMOTION_FLAG;
        if (promotionPT == PieceType::KNIGHT) return KNIGHT_PROMOTION_FLAG;
        if (promotionPT == PieceType::BISHOP) return BISHOP_PROMOTION_FLAG;
        if (promotionPT == PieceType::ROOK) return ROOK_PROMOTION_FLAG;
        return NORMAL_FLAG;
    }

    inline PieceType getPromotionPieceType()
    {
        return flagToPieceType(getTypeFlag());
    }

    inline string toUci()
    {
        string str = squareToStr[from()] + squareToStr[to()];
        uint16_t typeFlag = getTypeFlag();
        if (typeFlag == QUEEN_PROMOTION_FLAG) str += "q";
        else if (typeFlag == KNIGHT_PROMOTION_FLAG) str += "n";
        else if (typeFlag == BISHOP_PROMOTION_FLAG) str += "b";
        else if (typeFlag == ROOK_PROMOTION_FLAG) str += "r";
        return str;
    }
};

#endif