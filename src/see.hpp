
#pragma once

// clang-format off

namespace see { // SEE (Static exchange evaluation) 

                              // P    N    B    R    Q    K  NONE
const i32 SEE_PIECE_VALUES[7] = {100, 300, 300, 500, 900, 0, 0};
const u8 PAWN_INDEX = 0;

inline i32 gain(Board &board, Move move);

inline PieceType popLeastValuable(Board &board, u64 &occ, u64 attackers, Color color);

// SEE (Static exchange evaluation)
inline bool SEE(Board &board, Move move, i32 threshold = 0)
{
    i32 score = gain(board, move) - threshold;
    if (score < 0) return false;

    PieceType promotion = move.promotion();
    PieceType next = promotion != PieceType::NONE ? promotion : board.pieceTypeAt(move.from());
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0) return true;

    Square from = move.from();
    Square square = move.to();

    u64 occupancy = board.occupancy() ^ (1ULL << from) ^ (1ULL << square);
    u64 queens = board.bitboard(PieceType::QUEEN);
    u64 bishops = queens | board.bitboard(PieceType::BISHOP);
    u64 rooks = queens | board.bitboard(PieceType::ROOK);

    u64 attackers = 0;
    attackers |= (rooks & attacks::rookAttacks(square, occupancy));
    attackers |= (bishops & attacks::bishopAttacks(square, occupancy));
    attackers |= (board.bitboard(Color::BLACK, PieceType::PAWN) & attacks::pawnAttacks(square, Color::WHITE));
    attackers |= (board.bitboard(Color::WHITE, PieceType::PAWN) & attacks::pawnAttacks(square, Color::BLACK));
    attackers |= (board.bitboard(PieceType::KNIGHT) & attacks::knightAttacks(square));
    attackers |= (board.bitboard(PieceType::KING) & attacks::kingAttacks(square));

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

inline i32 gain(Board &board, Move move)
{
    auto moveFlag = move.flag();

    if (moveFlag == Move::CASTLING_FLAG)
        return 0;

    if (moveFlag == Move::EN_PASSANT_FLAG)
        return SEE_PIECE_VALUES[PAWN_INDEX];

    i32 score = SEE_PIECE_VALUES[(int)board.pieceTypeAt(move.to())];

    PieceType promotion = move.promotion();
    if (promotion != PieceType::NONE)
        // gain promotion, lose the pawn
        score += SEE_PIECE_VALUES[(int)promotion] - SEE_PIECE_VALUES[PAWN_INDEX]; 

    return score;
}

inline PieceType popLeastValuable(Board &board, u64 &occ, u64 attackers, Color color)
{
    for (int pt = 0; pt <= 5; pt++)
    {
        u64 bb = attackers & board.bitboard(color, (PieceType)pt);
        if (bb > 0)
        {
            occ ^= (1ULL << lsb(bb));
            return (PieceType)pt;
        }
    }

    return PieceType::NONE;
}

}

