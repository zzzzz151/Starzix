
#pragma once

// clang-format off

                                  // P    N    B    R    Q    K  NONE
const int32_t SEE_PIECE_VALUES[7] = {100, 300, 300, 500, 900, 0, 0};
const uint8_t PAWN_INDEX = 0;

inline int32_t gain(Board &board, Move move);

inline PieceType popLeastValuable(Board &board, uint64_t &occ, uint64_t attackers, Color color);

inline bool SEE(Board &board, Move move, int32_t threshold = 0)
{
    int32_t score = gain(board, move) - threshold;
    if (score < 0)
        return false;

    PieceType promotionPieceType = move.promotionPieceType();
    PieceType next = promotionPieceType != PieceType::NONE ? promotionPieceType : board.pieceTypeAt(move.from());
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0)
        return true;

    Square from = move.from();
    Square square = move.to();

    uint64_t occupancy = board.occupancy() ^ (1ULL << from) ^ (1ULL << square);
    uint64_t queens = board.getBitboard(PieceType::QUEEN);
    uint64_t bishops = queens | board.getBitboard(PieceType::BISHOP);
    uint64_t rooks = queens | board.getBitboard(PieceType::ROOK);

    uint64_t attackers = 0;
    attackers |= rooks & attacks::rookAttacks(square, occupancy);
    attackers |= bishops & attacks::bishopAttacks(square, occupancy);
    attackers |= board.getBitboard(PieceType::PAWN, BLACK) & attacks::pawnAttacks(square, WHITE);
    attackers |= board.getBitboard(PieceType::PAWN, WHITE) & attacks::pawnAttacks(square, BLACK);
    attackers |= board.getBitboard(PieceType::KNIGHT) & attacks::knightAttacks(square);
    attackers |= board.getBitboard(PieceType::KING) & attacks::kingAttacks(square);

    Color us = board.oppSide();
    while (true)
    {
        uint64_t ourAttackers = attackers & board.getBitboard(us);
        if (ourAttackers == 0)
            break;

        next = popLeastValuable(board, occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= attacks::bishopAttacks(square, occupancy) & bishops;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= attacks::rookAttacks(square, occupancy) & rooks;

        attackers &= occupancy;
        score = -score - 1 - SEE_PIECE_VALUES[(int)next];
        us = oppColor(us);

        if (score >= 0)
        {
            // if our only attacker is our king, but the opponent still has defenders
            if (next == PieceType::KING && (attackers & board.getBitboard(us)) > 0)
                us = oppColor(us);
            break;
        }
    }

    return board.sideToMove() != us;
}

inline int32_t gain(Board &board, Move move)
{
    auto moveFlag = move.typeFlag();

    if (moveFlag == Move::CASTLING_FLAG)
        return 0;

    if (moveFlag == Move::EN_PASSANT_FLAG)
        return SEE_PIECE_VALUES[PAWN_INDEX];

    int32_t score = SEE_PIECE_VALUES[(int)board.pieceTypeAt(move.to())];

    PieceType promotionPieceType = move.promotionPieceType();
    if (promotionPieceType != PieceType::NONE)
        score += SEE_PIECE_VALUES[(int)promotionPieceType] - SEE_PIECE_VALUES[PAWN_INDEX]; // gain promotion, lose the pawn

    return score;
}

inline PieceType popLeastValuable(Board &board, uint64_t &occ, uint64_t attackers, Color color)
{
    for (int pt = 0; pt <= 5; pt++)
    {
        uint64_t bb = attackers & board.getBitboard((PieceType)pt, color);
        if (bb > 0)
        {
            occ ^= (1ULL << lsb(bb));
            return (PieceType)pt;
        }
    }

    return PieceType::NONE;
}

