
#ifndef SEE_HPP
#define SEE_HPP

// clang-format off
#include "board/board.hpp"
#include "board/attacks.hpp"

const int SEE_PIECE_VALUES[7] = {100, 300, 300, 500, 900, 0, 0};
const int PAWN_INDEX = 0;

inline int gain(Board &board, Move &move)
{
    auto moveType = move.typeFlag();

    if (moveType == move.CASTLING_FLAG)
        return 0;

    if (moveType == move.EN_PASSANT_FLAG)
        return SEE_PIECE_VALUES[PAWN_INDEX];

    int score = SEE_PIECE_VALUES[(int)board.pieceTypeAt(move.to())];

    PieceType promotionPieceType = move.promotionPieceType();
    if (promotionPieceType != PieceType::NONE)
        score += SEE_PIECE_VALUES[(int)promotionPieceType] - SEE_PIECE_VALUES[PAWN_INDEX]; // gain promotion, lose the pawn

    return score;
}

inline PieceType popLeastValuable(Board &board, uint64_t &occ, uint64_t attackers, Color color)
{
    for (int pt = 0; pt <= 5; pt++)
    {
        uint64_t bb = attackers & board.pieceBitboard((PieceType)pt, color);
        if (bb > 0)
        {
            occ ^= (1ULL << lsb(bb));
            return (PieceType)pt;
        }
    }

    return PieceType::NONE;
}

inline bool SEE(Board &board, Move &move, int threshold = 0)
{
    int score = gain(board, move) - threshold;
    if (score < 0)
        return false;

    PieceType promotionPieceType = move.promotionPieceType();
    PieceType next = promotionPieceType != PieceType::NONE ? promotionPieceType : board.pieceTypeAt(move.from());
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0)
        return true;

    int from = (int)move.from();
    int to = (int)move.to();

    uint64_t occupancy = board.occupancy() ^ (1ULL << from) ^ (1ULL << to);
    uint64_t queens = board.pieceBitboard(PieceType::QUEEN);
    uint64_t bishops = queens | board.pieceBitboard(PieceType::BISHOP);
    uint64_t rooks = queens | board.pieceBitboard(PieceType::ROOK);

    Square square = move.to();

    uint64_t attackers = 0;
    attackers |= rooks & rookAttacks(square, occupancy);
    attackers |= bishops & bishopAttacks(square, occupancy);
    attackers |= board.pieceBitboard(PieceType::PAWN, BLACK) & pawnAttacks(square, WHITE);
    attackers |= board.pieceBitboard(PieceType::PAWN, WHITE) & pawnAttacks(square, BLACK);
    attackers |= board.pieceBitboard(PieceType::KNIGHT) & knightMoves[square];
    attackers |= board.pieceBitboard(PieceType::KING) & kingMoves[square];

    Color us = board.enemyColor();
    while (true)
    {
        uint64_t ourAttackers = attackers & board.colorBitboard(us);
        if (ourAttackers == 0)
            break;

        next = popLeastValuable(board, occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= bishopAttacks(square, occupancy) & bishops;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= rookAttacks(square, occupancy) & rooks;

        attackers &= occupancy;
        score = -score - 1 - SEE_PIECE_VALUES[(int)next];
        us = oppColor(us);

        if (score >= 0)
        {
            // if our only attacker is our king, but the opponent still has defenders
            if (next == PieceType::KING && (attackers & board.colorBitboard(us)) > 0)
                us = oppColor(us);
            break;
        }
    }

    return board.colorToMove() != us;
}

#endif