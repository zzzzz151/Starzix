#ifndef SEE_HPP
#define SEE_HPP

#include "chess.hpp"
#include "search.hpp"
#include "eval.hpp"
using namespace chess;
using namespace std;

inline int squareBit(Square sq)
{
    return U64(1) << (int)sq;
}

inline int gain(Move move)
{
    if (move.typeOf() == move.CASTLING)
        return 0;

    if (move.typeOf() == move.ENPASSANT)
        return PIECE_VALUES[1];

    int score = PIECE_VALUES[(int)board.at<PieceType>(move.to())];

    if (move.typeOf() == move.PROMOTION)
        score += PIECE_VALUES[(int)move.promotionType()] - PIECE_VALUES[1]; // gain promotion, lose the pawn

    return score;
}


inline PieceType popLeastValuable(Bitboard &occ, Bitboard attackers, Color color)
{
    Bitboard bb = attackers & board.pieces(PieceType::PAWN, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::PAWN;
    }

    bb = attackers & board.pieces(PieceType::KNIGHT, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::KNIGHT;
    }

    bb = attackers & board.pieces(PieceType::BISHOP, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::BISHOP;
    }

    bb = attackers & board.pieces(PieceType::ROOK, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::ROOK;
    }

    bb = attackers & board.pieces(PieceType::QUEEN, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::QUEEN;
    }

    bb = attackers & board.pieces(PieceType::KING, color);
    if (bb > 0)
    {
        occ ^= 1ULL << builtin::lsb(bb);
        return PieceType::KING;
    }

    return PieceType::NONE;
}

// SEE (static exchange evaluation)
inline bool SEE(Move move, int threshold = 0)
{
    int score = gain(move) - threshold;
    if (score < 0)
        return false;

    Square square = move.to();
    PieceType next = move.typeOf() == move.PROMOTION ? move.promotionType() : board.at<PieceType>(move.from());
    score -= PIECE_VALUES[(int)next];
    if (score >= 0)
        return true;

    Bitboard occupancy = board.occ() ^ squareBit(move.from()) ^ squareBit(square);
    Bitboard queens = board.pieces(PieceType::QUEEN);
    Bitboard bishops = queens | board.pieces(PieceType::BISHOP);
    Bitboard rooks = queens | board.pieces(PieceType::ROOK);

    Bitboard attackers = U64(0);
    attackers |= rooks & movegen::attacks::rook(square, occupancy);
    attackers |= bishops & movegen::attacks::bishop(square, occupancy);
    attackers |= board.pieces(PieceType::PAWN, Color::BLACK) & movegen::attacks::pawn(Color::WHITE, square);
    attackers |= board.pieces(PieceType::PAWN, Color::WHITE) & movegen::attacks::pawn(Color::BLACK, square);
    attackers |= board.pieces(PieceType::KNIGHT) & movegen::attacks::knight(square);
    attackers |= board.pieces(PieceType::KING) & movegen::attacks::king(square);

    Color us = ~board.sideToMove();
    while (true)
    {
        Bitboard ourAttackers = attackers & board.us(us);
        if (ourAttackers == 0)
            break;

        next = popLeastValuable(occupancy, ourAttackers, us);

        if (next == PieceType::PAWN || next == PieceType::BISHOP || next == PieceType::QUEEN)
            attackers |= movegen::attacks::bishop(square, occupancy) & bishops;

        if (next == PieceType::ROOK || next == PieceType::QUEEN)
            attackers |= movegen::attacks::rook(square, occupancy) & rooks;

        attackers &= occupancy;
        score = -score - 1 - PIECE_VALUES[(int)next];
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