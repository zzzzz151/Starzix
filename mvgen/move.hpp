#ifndef MOVE_HPP
#define MOVE_HPP

// clang-format off
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
using namespace std;

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
    uint16_t moveEncoded = 0;

    Move() = default;

    // Custom == operator
    bool operator==(const Move& other) const {
        return moveEncoded == other.moveEncoded;
    }

    inline Move(Square from, Square to, PieceType pieceType, PieceType promotionPieceType = PieceType::QUEEN, bool isEnPassant = false)//, bool isEnPassant = false)
    {
        // from/to: 00100101
        // (uint16_t)from/to: 00000000 00100101

        moveEncoded = ((uint16_t)from << 10);
        moveEncoded |= ((uint16_t)to << 4);

        if (isEnPassant) 
            moveEncoded |= EN_PASSANT_FLAG;
        else if (pieceType == PieceType::PAWN)
        {
            uint8_t rank = squareRank(to);
            if (!isBackRank(rank)) return;
            if (promotionPieceType == PieceType::QUEEN) moveEncoded |= QUEEN_PROMOTION_FLAG;
            else if (promotionPieceType == PieceType::KNIGHT) moveEncoded |=  KNIGHT_PROMOTION_FLAG;
            else if (promotionPieceType == PieceType::BISHOP) moveEncoded |= BISHOP_PROMOTION_FLAG;
            else if (promotionPieceType == PieceType::ROOK) moveEncoded |= ROOK_PROMOTION_FLAG;
        }
        else if (pieceType == PieceType::KING)
        {
            // white castle
            if (from == 4 && (to == 6 || to == 2))
                moveEncoded |= CASTLING_FLAG;
            // black castle
            else if (from == 60 && (to == 62 || to == 58))
                moveEncoded |= CASTLING_FLAG;
        }
        
    }

    inline Move (string from, string to, PieceType pieceType) : Move(strToSquare(from), strToSquare(to), pieceType) {}

    static inline Move fromUci(string uci, Piece* boardPieces)
    {
        Square from = strToSquare(uci.substr(0,2));
        Square to = strToSquare(uci.substr(2,4));
        PieceType pieceType = pieceTypeAt(from, boardPieces);

        if (uci.size() == 5) // promotion
        {
            char promotionLowerCase = uci.back(); // last char of string
            PieceType promotionPieceType = PieceType::QUEEN;
            if (promotionLowerCase == 'n') promotionPieceType = PieceType::KNIGHT;
            else if (promotionLowerCase == 'b') promotionPieceType = PieceType::BISHOP;
            else if (promotionLowerCase == 'r') promotionPieceType = PieceType::ROOK;
            return Move(from, to, pieceType, promotionPieceType);
        }

        if (pieceType == PieceType::PAWN && boardPieces[to] == Piece::NONE && squareFile(from) != squareFile(to))
            return Move(from, to, PieceType::PAWN, PieceType::NONE, true);

        return Move(from, to, pieceType);
    }

    inline Square from()
    {
        return (moveEncoded >> 10) & 0b111111;
    }

    inline Square to()
    {
        return (moveEncoded >> 4) & 0b111111;
    }

    inline uint16_t getTypeFlag()
    {
        return moveEncoded & 0x000F;
    }

    inline PieceType getPromotionPieceType()
    {
        uint16_t typeFlag = getTypeFlag();
        if (typeFlag == QUEEN_PROMOTION_FLAG) return PieceType::QUEEN;
        if (typeFlag == KNIGHT_PROMOTION_FLAG) return PieceType::KNIGHT;
        if (typeFlag == BISHOP_PROMOTION_FLAG) return PieceType::BISHOP;
        if (typeFlag == ROOK_PROMOTION_FLAG) return PieceType::ROOK;
        return PieceType::NONE;
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

#include "board.hpp"

#endif