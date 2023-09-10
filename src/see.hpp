
#ifndef SEE_HPP
#define SEE_HPP

#include "chess.hpp"
#include "search.hpp" // contains SEE_PIECE_VALUES and PAWN_INDEX
extern const int SEE_PIECE_VALUES[7], PAWN_INDEX;
using namespace chess;

inline int gain(const Board &board, Move &move)
{
    auto moveType = move.typeOf();

    if (moveType == move.CASTLING)
        return 0;

    if (moveType == move.ENPASSANT)
        return SEE_PIECE_VALUES[PAWN_INDEX];

    int score = SEE_PIECE_VALUES[(int)board.at<PieceType>(move.to())];

    if (moveType == move.PROMOTION)
        score += SEE_PIECE_VALUES[(int)move.promotionType()] - SEE_PIECE_VALUES[PAWN_INDEX]; // gain promotion, lose the pawn

    return score;
}

inline PieceType popLeastValuable(const Board &board, Bitboard &occ, Bitboard attackers, Color color)
{
    for (int pt = 0; pt <= 5; pt++)
    {
        Bitboard bb = attackers & board.pieces((PieceType)pt, color);
        if (bb > 0)
        {
            occ ^= (1ULL << builtin::lsb(bb));
            return (PieceType)pt;
        }
    }

    return PieceType::NONE;
}

inline bool SEE(const Board &board, Move &move, int threshold = 0)
{
    int score = gain(board, move) - threshold;
    if (score < 0)
        return false;

    PieceType next = move.typeOf() == move.PROMOTION ? move.promotionType() : board.at<PieceType>(move.from());
    score -= SEE_PIECE_VALUES[(int)next];
    if (score >= 0)
        return true;

    int from = (int)move.from();
    int to = (int)move.to();

    Bitboard occupancy = board.occ() ^ (1ULL << from) ^ (1ULL << to);
    Bitboard queens = board.pieces(PieceType::QUEEN);
    Bitboard bishops = queens | board.pieces(PieceType::BISHOP);
    Bitboard rooks = queens | board.pieces(PieceType::ROOK);

    Square square = move.to();

    Bitboard attackers = 0;
    attackers |= rooks & attacks::rook(square, occupancy);
    attackers |= bishops & attacks::bishop(square, occupancy);
    attackers |= board.pieces(PieceType::PAWN, Color::BLACK) & attacks::pawn(Color::WHITE, square);
    attackers |= board.pieces(PieceType::PAWN, Color::WHITE) & attacks::pawn(Color::BLACK, square);
    attackers |= board.pieces(PieceType::KNIGHT) & attacks::knight(square);
    attackers |= board.pieces(PieceType::KING) & attacks::king(square);

    Color us = ~board.sideToMove();
    while (true)
    {
        Bitboard ourAttackers = attackers & board.us(us);
        if (ourAttackers == 0)
            break;

        next = popLeastValuable(board, occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= attacks::bishop(square, occupancy) & bishops;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= attacks::rook(square, occupancy) & rooks;

        attackers &= occupancy;
        score = -score - 1 - SEE_PIECE_VALUES[(int)next];
        us = ~us;

        if (score >= 0)
        {
            // if our only attacker is our king, but the opponent still has defenders
            if (next == PieceType::KING && (attackers & board.us(us)) > 0)
                us = ~us;
            break;
        }
    }

    return board.sideToMove() != us;
}

#endif