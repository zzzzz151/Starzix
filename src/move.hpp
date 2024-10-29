// clang-format off

#pragma once

#include "utils.hpp"

struct Move {
    private:

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)
    u16 mMove = 0;

    public:

    constexpr static u16
        NULL_FLAG             = 0x0000,
        PAWN_FLAG             = 0x0001,
        KNIGHT_FLAG           = 0x0002,
        BISHOP_FLAG           = 0x0003,
        ROOK_FLAG             = 0x0004,
        QUEEN_FLAG            = 0x0005,
        KING_FLAG             = 0x0006,
        PAWN_TWO_UP_FLAG      = 0x0007,
        EN_PASSANT_FLAG       = 0x0008,
        KNIGHT_PROMOTION_FLAG = 0x000A,
        BISHOP_PROMOTION_FLAG = 0x000B,
        ROOK_PROMOTION_FLAG   = 0x000C,
        QUEEN_PROMOTION_FLAG  = 0x000D,
        CASTLING_FLAG         = 0x000F;

    constexpr bool operator==(const Move other) const {
        return mMove == other.mMove;
    }

    constexpr bool operator!=(const Move other) const {
        return mMove != other.mMove;
    }

    constexpr Move() = default;

    constexpr Move(const u16 move) { mMove = move; }

    constexpr Move(const Square from, const Square to, const u16 flag)
    {
        assert(from <= 63 && to <= 63);
        mMove = (u16)from << 10;
        mMove |= (u16)to << 4;
        mMove |= flag;

        // No pawns in backranks
        assert(!(
            pieceType() == PieceType::PAWN
            && (squareRank(to) == Rank::RANK_1 || squareRank(to) == Rank::RANK_8)
            && promotion() == PieceType::NONE
        ));
    }

    inline Move(std::string from, std::string to, u16 flag)
    : Move(strToSquare(from), strToSquare(to), flag) { }

    constexpr u16 encoded() const { return mMove; }

    constexpr Square from() const { return (mMove >> 10) & 0b111111; }

    constexpr Square to() const { return (mMove >> 4) & 0b111111; }

    constexpr u16 flag() const { return mMove & 0x000F; }

    constexpr PieceType pieceType() const
    {
        const auto flag = this->flag();
        if (flag == NULL_FLAG)
            return PieceType::NONE;
        if (flag <= KING_FLAG)
            return (PieceType)(flag - 1);
        if (flag == CASTLING_FLAG)
            return PieceType::KING;
        return PieceType::PAWN;
    }

    constexpr PieceType promotion() const
    {
        const u16 flag = this->flag();

        if (flag >= KNIGHT_PROMOTION_FLAG && flag <= QUEEN_PROMOTION_FLAG)
            return (PieceType)(flag - KNIGHT_PROMOTION_FLAG + 1);

        return PieceType::NONE;
    }

    inline std::string toUci() const
    {
        std::string str = SQUARE_TO_STR[from()] + SQUARE_TO_STR[to()];
        const u16 flag = this->flag();

        if (flag == QUEEN_PROMOTION_FLAG)
            str += "q";
        else if (flag == KNIGHT_PROMOTION_FLAG)
            str += "n";
        else if (flag == BISHOP_PROMOTION_FLAG)
            str += "b";
        else if (flag == ROOK_PROMOTION_FLAG)
            str += "r";

        return str;
    }

}; // struct Move

static_assert(sizeof(Move) == 2); // 2 bytes

constexpr Move MOVE_NONE = Move();
