// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"

struct CuckooTable {
    public:
    std::array<u64,  8192> keys  = { };
    std::array<Move, 8192> moves = { };
};

constexpr u64 cuckoo_h1(const u64 key) { return key & 0x1fff; }

constexpr u64 cuckoo_h2(const u64 key) { return (key >> 16) & 0x1fff; }

constexpr CuckooTable CUCKOO_TABLE = []() consteval
{
    CuckooTable cuckooTable = { };

    [[maybe_unused]] int count = 0;

    auto initPiece = [&](const Color color, const PieceType pieceType) consteval -> void
    {
        for (Square sq1 = 0; sq1 < 64; sq1++)
            for (Square sq2 = sq1 + 1; sq2 < 64; sq2++)
            {
                const u64 attacks = pieceType == PieceType::KNIGHT ? getKnightAttacks(sq1)
                                  : pieceType == PieceType::BISHOP ? getBishopAttacks(sq1, 0)
                                  : pieceType == PieceType::ROOK   ? getRookAttacks(sq1, 0)
                                  : pieceType == PieceType::QUEEN  ? getQueenAttacks(sq1, 0)
                                  : getKingAttacks(sq1);

                if ((attacks & bitboard(sq2)) == 0) continue;

                Move move = Move(sq1, sq2, (u16)pieceType + 1);

                u64 key = ZOBRIST_COLOR
                        ^ ZOBRIST_PIECES[(int)color][(int)pieceType][sq1]
                        ^ ZOBRIST_PIECES[(int)color][(int)pieceType][sq2];

                u64 idx = cuckoo_h1(key);

                while (move != MOVE_NONE) {
                    std::swap(cuckooTable.keys[idx], key);
                    std::swap(cuckooTable.moves[idx], move);
                    idx = idx == cuckoo_h1(key) ? cuckoo_h2(key) : cuckoo_h1(key);
                }

                count++;
            }
    };

    for (const Color color : {Color::WHITE, Color::BLACK})
        for (int pt = KNIGHT; pt <= KING; pt++)
            initPiece(color, (PieceType)pt);

    assert(count == 3668);
    return cuckooTable;
}();
