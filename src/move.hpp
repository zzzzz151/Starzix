// clang-format off

#pragma once

#include "types.hpp"
#include "utils.hpp"
#include <optional>

enum class MoveFlag : u16 {
    None = 0, Pawn = 1, Knight = 2, Bishop = 3, Rook = 4, Queen = 5, King = 6,
    PawnDoublePush = 7, EnPassant = 8, Castling = 9,
    KnightPromo = 10, BishopPromo = 11, RookPromo = 12, QueenPromo = 13,
    Count = 14
};

struct Move
{
private:

    // 16 bits: ffffff tttttt FFFF (f = from, t = to, F = flag)
    u16 mMove = 0;

public:

    constexpr operator bool() const { return mMove != 0; }

    constexpr bool operator==(const Move other) const
    {
        return mMove == other.mMove;
    }

    constexpr bool operator!=(const Move other) const
    {
        return mMove != other.mMove;
    }

    constexpr Move() = default;

    constexpr Move(const u16 move) {
        mMove = move;
    }

    constexpr Move(const Square from, const Square to, const MoveFlag flag)
    {
        mMove = static_cast<u16>(static_cast<u16>(from) << 10);
        mMove |= static_cast<u16>(to) << 4;
        mMove |= static_cast<u16>(flag);

        // No pawns in backranks
        assert(pieceType() != PieceType::Pawn || !isBackrank(squareRank(from)));

        // Pawn must promote
        assert(pieceType() != PieceType::Pawn
            || !isBackrank(squareRank(to))
            || promotion().has_value()
        );
    }

    inline Move(std::string from, std::string to, MoveFlag flag)
        : Move(strToSquare(from), strToSquare(to), flag) { }

    constexpr u16 asU16() const {
        return mMove;
    }

    constexpr Square from() const
    {
        assert(flag() != MoveFlag::None);
        return asEnum<Square>((mMove >> 10) & 0b111111);
    }

    constexpr Square to() const
    {
        assert(flag() != MoveFlag::None);
        return asEnum<Square>((mMove >> 4) & 0b111111);
    }

    constexpr MoveFlag flag() const {
        return asEnum<MoveFlag>(mMove & 0x000f);
    }

    constexpr PieceType pieceType() const
    {
        const MoveFlag flag = this->flag();
        assert(flag != MoveFlag::None);

        if (static_cast<i32>(flag) <= static_cast<i32>(MoveFlag::King))
            return asEnum<PieceType>(static_cast<i32>(flag) - 1);

        if (flag == MoveFlag::Castling)
            return PieceType::King;

        return PieceType::Pawn;
    }

    constexpr std::optional<PieceType> promotion() const
    {
        assert(flag() != MoveFlag::None);
        const i32 flag = static_cast<i32>(this->flag());

        if (flag >= static_cast<i32>(MoveFlag::KnightPromo)
        &&  flag <= static_cast<i32>(MoveFlag::QueenPromo))
            return asEnum<PieceType>(flag - static_cast<i32>(MoveFlag::KnightPromo) + 1);

        return std::nullopt;
    }

    constexpr bool isUnderpromotion() const
    {
        const std::optional<PieceType> promo = promotion();
        return promo.has_value() && promo != PieceType::Queen;
    }

    inline std::string toUci() const
    {
        if (flag() == MoveFlag::None)
            return "0000";

        std::string str = squareToStr(from()) + squareToStr(to());
        const MoveFlag flag = this->flag();

        if (flag == MoveFlag::QueenPromo)
            str += "q";
        else if (flag == MoveFlag::KnightPromo)
            str += "n";
        else if (flag == MoveFlag::BishopPromo)
            str += "b";
        else if (flag == MoveFlag::RookPromo)
            str += "r";

        return str;
    }

}; // struct Move

constexpr Move MOVE_NONE = Move();

static_assert(sizeof(Move) == 2); // 2 bytes

// Test Move.from()
static_assert(Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush).from() == Square::D2);
static_assert(Move(Square::D5, Square::E6, MoveFlag::EnPassant)     .from() == Square::D5);
static_assert(Move(Square::A2, Square::A1, MoveFlag::QueenPromo)    .from() == Square::A2);
static_assert(Move(Square::B1, Square::C3, MoveFlag::Knight)        .from() == Square::B1);

// Test Move.to()
static_assert(Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush).to() == Square::D4);
static_assert(Move(Square::D5, Square::E6, MoveFlag::EnPassant)     .to() == Square::E6);
static_assert(Move(Square::A2, Square::A1, MoveFlag::QueenPromo)    .to() == Square::A1);
static_assert(Move(Square::B1, Square::C3, MoveFlag::Knight)        .to() == Square::C3);

// Test Move.pieceType()

static_assert(Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush)
    .pieceType() == PieceType::Pawn);

static_assert(Move(Square::D5, Square::E6, MoveFlag::EnPassant)
    .pieceType() == PieceType::Pawn);

static_assert(Move(Square::A2, Square::A1, MoveFlag::QueenPromo)
    .pieceType() == PieceType::Pawn);

static_assert(Move(Square::B1, Square::C3, MoveFlag::Knight)
    .pieceType() == PieceType::Knight);

static_assert(Move(Square::E1, Square::G1, MoveFlag::Castling)
    .pieceType() == PieceType::King);

// Test Move.promotion()

static_assert(!Move(Square::D4, Square::D5, MoveFlag::Pawn)          .promotion().has_value());
static_assert(!Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush).promotion().has_value());
static_assert(!Move(Square::D5, Square::E6, MoveFlag::EnPassant)     .promotion().has_value());

static_assert(Move(Square::A2, Square::A1, MoveFlag::KnightPromo)
    .promotion() == PieceType::Knight);

static_assert(Move(Square::A2, Square::A1, MoveFlag::QueenPromo)
    .promotion() == PieceType::Queen);

static_assert(!Move(Square::B1, Square::C3, MoveFlag::Knight)  .promotion().has_value());
static_assert(!Move(Square::E1, Square::G1, MoveFlag::Castling).promotion().has_value());

// Test Move.isUnderpromotion()
static_assert(!Move(Square::D4, Square::D5, MoveFlag::Pawn)      .isUnderpromotion());
static_assert(Move(Square::A2, Square::A1, MoveFlag::KnightPromo).isUnderpromotion());
static_assert(!Move(Square::A2, Square::A1, MoveFlag::QueenPromo).isUnderpromotion());
static_assert(!Move(Square::B1, Square::C3, MoveFlag::Knight)    .isUnderpromotion());

/*
// Test Move.toUci()
static_assert(Move(Square::D5, Square::E6, MoveFlag::EnPassant)  .toUci() == "d5e6");
static_assert(Move(Square::A2, Square::A1, MoveFlag::KnightPromo).toUci() == "a2a1n");
static_assert(Move(Square::A2, Square::A1, MoveFlag::QueenPromo) .toUci() == "a2a1q");
static_assert(Move(Square::E1, Square::G1, MoveFlag::Castling)   .toUci() == "e1g1");
*/
