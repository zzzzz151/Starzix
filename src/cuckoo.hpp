// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include <algorithm>
#include <stdexcept>

constexpr u64 h1(const u64 hash)
{
    return hash & 0x1fff;
}

constexpr u64 h2(const u64 hash)
{
    return (hash >> 16) & 0x1fff;
}

struct CuckooData
{
public:
    std::array<u64, 8192>  hashes = { };
    std::array<Move, 8192> moves  = { };
};

constexpr CuckooData CUCKOO_DATA = [] () consteval
{
    CuckooData cuckooData = { };
    size_t count = 0;

    const auto processFeature = [&] (
        const Color pieceColor, const PieceType pt, const Square square) consteval
    {
        if (pt == PieceType::Pawn)
            return;

        const Bitboard attacks
            = pt == PieceType::Knight ? getKnightAttacks(square)
            : pt == PieceType::Bishop ? getBishopAttacks(square, 0)
            : pt == PieceType::Rook   ? getRookAttacks  (square, 0)
            : pt == PieceType::Queen  ? getQueenAttacks (square, 0)
            : getKingAttacks(square);

        Square square2 = square;

        while (square2 != Square::H8)
        {
            square2 = next<Square>(square2);

            if (!hasSquare(attacks, square2))
                continue;

            const MoveFlag flag = asEnum<MoveFlag>(static_cast<i32>(pt) + 1);
            Move move = Move(square, square2, flag);

            u64 hash = ZOBRIST_COLOR
                     ^ ZOBRIST_PIECES[pieceColor][pt][square]
                     ^ ZOBRIST_PIECES[pieceColor][pt][square2];

            auto i = h1(hash);

            while (true)
            {
                std::swap(cuckooData.hashes[i], hash);
                std::swap(cuckooData.moves[i], move);

                // Arrived at empty slot?
                if (move == MOVE_NONE) break;

                // Push victim to alternative slot
                i = i == h1(hash) ? h2(hash) : h1(hash);
            }

            count++;
        }
    };

    for (const Color pieceColor : EnumIter<Color>())
        for (const PieceType pt : EnumIter<PieceType>())
        {
            if (pt == PieceType::Pawn)
                continue;

            for (const Square square : EnumIter<Square>())
                processFeature(pieceColor, pt, square);
        }

    if (count != 3668)
        throw std::logic_error("Wrong cuckoo count, expected 3668");

    return cuckooData;
}();
