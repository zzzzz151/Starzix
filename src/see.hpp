
// clang-format off

#pragma once

#include "search_params.hpp"

std::array<i32, 7> SEE_PIECE_VALUES = {
        seePawnValue(),  // Pawn
        seeMinorValue(), // Knight
        seeMinorValue(), // Bishop
        seeRookValue(),  // Rook
        seeQueenValue(), // Queen
        0,               // King
        0                // None
};

inline PieceType popLeastValuable(Board &board, u64 &occ, u64 attackers, Color color)
{
    for (int pt = 0; pt <= 5; pt++)
    {
        u64 bb = attackers & board.bitboard(color, (PieceType)pt);
        if (bb > 0) {
            occ ^= 1ULL << lsb(bb);
            return (PieceType)pt;
        }
    }

    return PieceType::NONE;
}

// SEE (Static exchange evaluation)
inline bool SEE(Board &board, Move move, i32 threshold = 0)
{
    assert(move != MOVE_NONE);

    i32 score = -threshold;

    PieceType captured = board.captured(move);
    score += SEE_PIECE_VALUES[(int)captured];

    PieceType promotion = move.promotion();

    if (promotion != PieceType::NONE)
        score += SEE_PIECE_VALUES[(int)promotion] 
                 - SEE_PIECE_VALUES[(int)PieceType::PAWN];

    if (score < 0) return false;

    PieceType next = promotion != PieceType::NONE ? promotion : move.pieceType();
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0) return true;

    Square from = move.from();
    Square square = move.to();

    u64 occupancy = board.occupancy() ^ (1ULL << from) ^ (1ULL << square);
    u64 queens = board.bitboard(PieceType::QUEEN);
    u64 bishops = queens | board.bitboard(PieceType::BISHOP);
    u64 rooks = queens | board.bitboard(PieceType::ROOK);

    u64 attackers = rooks & attacks::rookAttacks(square, occupancy);
    attackers |= bishops & attacks::bishopAttacks(square, occupancy);

    attackers |= board.bitboard(Color::BLACK, PieceType::PAWN) 
                 & attacks::pawnAttacks(square, Color::WHITE);

    attackers |= board.bitboard(Color::WHITE, PieceType::PAWN) 
                 & attacks::pawnAttacks(square, Color::BLACK);

    attackers |= board.bitboard(PieceType::KNIGHT) & attacks::knightAttacks(square);
    attackers |= board.bitboard(PieceType::KING) & attacks::kingAttacks(square);

    Color us = board.oppSide();
    while (true)
    {
        u64 ourAttackers = attackers & board.bitboard(us);
        if (ourAttackers == 0) break;

        next = popLeastValuable(board, occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= attacks::bishopAttacks(square, occupancy) & bishops;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= attacks::rookAttacks(square, occupancy) & rooks;

        attackers &= occupancy;
        score = -score - 1 - SEE_PIECE_VALUES[(int)next];
        us = oppColor(us);

        // if our only attacker is our king, but the opponent still has defenders
        if (score >= 0 && next == PieceType::KING 
        && (attackers & board.bitboard(us)) > 0)
            us = oppColor(us);

        if (score >= 0) break;
    }

    return board.sideToMove() != us;
}
