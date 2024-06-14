
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
        u64 bb = attackers & board.getBb(color, (PieceType)pt);
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
                 - SEE_PIECE_VALUES[PAWN];

    if (score < 0) return false;

    PieceType next = promotion != PieceType::NONE ? promotion : move.pieceType();
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0) return true;

    Square from = move.from();
    Square square = move.to();

    u64 bishopsQueens = board.getBb(PieceType::BISHOP) | board.getBb(PieceType::QUEEN);
    u64 rooksQueens   = board.getBb(PieceType::ROOK)   | board.getBb(PieceType::QUEEN);

    u64 occupancy = board.occupancy() ^ (1ULL << from) ^ (1ULL << square);
    u64 attackers = board.attackers(square, occupancy);

    Color us = board.oppSide();

    while (true)
    {
        u64 ourAttackers = attackers & board.getBb(us);
        if (ourAttackers == 0) break;

        next = popLeastValuable(board, occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= attacks::bishopAttacks(square, occupancy) & bishopsQueens;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= attacks::rookAttacks(square, occupancy) & rooksQueens;

        attackers &= occupancy;
        score = -score - 1 - SEE_PIECE_VALUES[(int)next];
        us = oppColor(us);

        // if our only attacker is our king, but the opponent still has defenders
        if (score >= 0 && next == PieceType::KING 
        && (attackers & board.getBb(us)) > 0)
            us = oppColor(us);

        if (score >= 0) break;
    }

    return board.sideToMove() != us;
}
