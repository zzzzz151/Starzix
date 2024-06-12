// clang-format off

#pragma once

namespace cuckoo {

std::array<u64, 8192> KEYS = {};
std::array<Move, 8292> MOVES = {};

constexpr u64 h1(u64 key) { return key & 0x1fff; }

constexpr u64 h2(u64 key) { return (key >> 16) & 0x1fff; }

constexpr int initPiece(Color color, PieceType pieceType) 
{
    assert(pieceType != PieceType::PAWN);

    int count = 0;

    for (Square sq1 = 0; sq1 < 64; sq1++)
        for (Square sq2 = sq1 + 1; sq2 < 64; sq2++)
        {
            u64 attacks = pieceType == PieceType::KNIGHT ? attacks::knightAttacks(sq1)
                        : pieceType == PieceType::BISHOP ? attacks::bishopAttacks(sq1, 0)
                        : pieceType == PieceType::ROOK   ? attacks::rookAttacks(sq1, 0)
                        : pieceType == PieceType::QUEEN  ? attacks::queenAttacks(sq1, 0)
                        : attacks::kingAttacks(sq1);

            if ((attacks & (1ULL << sq2)) == 0) continue;

            Move move = Move(sq1, sq2, (u16)pieceType + 1);

            u64 key = ZOBRIST_COLOR
                      ^ ZOBRIST_PIECES[(int)color][(int)pieceType][sq1] 
                      ^ ZOBRIST_PIECES[(int)color][(int)pieceType][sq2];

            u64 idx = h1(key);

            while (move != MOVE_NONE) {
                std::swap(KEYS[idx], key);
                std::swap(MOVES[idx], move);
                idx = idx == h1(key) ? h2(key) : h1(key);
            }

            count++;
        }

    return count;
}

constexpr void init()
{
    int count = 0;

    for (Color color : {Color::WHITE, Color::BLACK})
        for (int pt = (int)PieceType::KNIGHT; pt <= (int)PieceType::KING; pt++)
            count += initPiece(color, (PieceType)pt);

    assert(count == 3668);

    if (count != 3668) exit(EXIT_FAILURE);
}

} // namespace cuckoo