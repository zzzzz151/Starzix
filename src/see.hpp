
#ifndef SEE_HPP
#define SEE_HPP

#include "chess.hpp"
using namespace chess;
using namespace std;

const int SEE_PIECE_VALUES[7] = {93, 308, 346, 521, 994, 20000, 0};

inline bool SEE(Board &board, Move move, int threshold = 0)
{
    int from = (int)move.from();
    int to = (int)move.to();

    PieceType target = board.at<PieceType>(move.to());
    int value = SEE_PIECE_VALUES[(int)target] - threshold;
    if (value < 0)
        return false;

    PieceType attacker = board.at<PieceType>(move.from());
    value -= SEE_PIECE_VALUES[(int)attacker];
    if (value >= 0)
        return true;

    U64 occupied = (board.occ() ^ (1ULL << from)) | (1ULL << to);
    U64 attackers = attacks::attackers(board, Color::WHITE, move.to(), occupied) | attacks::attackers(board, Color::BLACK, move.to(), occupied);
    attackers &= occupied;
    U64 queens = board.pieces(PieceType::QUEEN);
    U64 bishops = board.pieces(PieceType::BISHOP) | queens;
    U64 rooks = board.pieces(PieceType::ROOK) | queens;
    Color stm = ~board.sideToMove();

    while (true)
    {
        attackers &= occupied;
        U64 myAttackers = attackers & board.us(stm);
        if (myAttackers == 0)
            break;

        int pt;
        for (pt = 0; pt <= 5; pt++)
            if ((myAttackers & board.pieces((PieceType)pt)) > 0)
                break;

        stm = ~stm;
        if ((value = -value - 1 - SEE_PIECE_VALUES[pt]) >= 0)
        {
            if ((PieceType)pt == PieceType::KING && (attackers & board.us(stm)) > 0)
                stm = ~stm;
            break;
        }

        U64 x = builtin::lsb(myAttackers & board.pieces((PieceType)pt));
        occupied ^= (1ULL << x);

        if ((PieceType)pt == PieceType::PAWN || (PieceType)pt == PieceType::BISHOP || (PieceType)pt == PieceType::QUEEN)
            attackers |= attacks::bishop(move.to(), occupied) & bishops;
        if ((PieceType)pt == PieceType::ROOK || (PieceType)pt == PieceType::QUEEN)
            attackers |= attacks::rook(move.to(), occupied) & rooks;
    }

    return stm != board.sideToMove();
}

#endif