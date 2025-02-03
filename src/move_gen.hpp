// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "position.hpp"

enum class MoveGenType : i32 {
    AllMoves = 0, NoisyOnly = 1, QuietOnly = 2
};

constexpr void addPromotions(
    ArrayVec<Move, 256>& moves,
    const Square from,
    const Square to,
    const bool underpromos)
{
    moves.push_back(Move(from, to, MoveFlag::QueenPromo));

    if (underpromos) {
        moves.push_back(Move(from, to, MoveFlag::KnightPromo));
        moves.push_back(Move(from, to, MoveFlag::RookPromo));
        moves.push_back(Move(from, to, MoveFlag::BishopPromo));
    }
}

constexpr ArrayVec<Move, 256> pseudolegalMoves(
    const Position& pos,
    const MoveGenType moveGenType,
    const bool underpromos = true,
    const std::optional<Bitboard> optEnemyAttacks = std::nullopt)
{
    ArrayVec<Move, 256> pseudolegals;

    const Color stm = pos.sideToMove();

    const Bitboard ourPawns   = pos.getBb(stm, PieceType::Pawn);
    const Bitboard ourKnights = pos.getBb(stm, PieceType::Knight);
    const Bitboard ourBishops = pos.getBb(stm, PieceType::Bishop);
    const Bitboard ourRooks   = pos.getBb(stm, PieceType::Rook);
    const Bitboard ourQueens  = pos.getBb(stm, PieceType::Queen);

    // En passant
    if (moveGenType != MoveGenType::QuietOnly && pos.enPassantSquare())
    {
        const Square enPassantSquare = *(pos.enPassantSquare());

        const Bitboard ourEpPawns
            = ourPawns & getPawnAttacks(enPassantSquare, pos.notSideToMove());

        ITERATE_BITBOARD(ourEpPawns, ourPawnSquare,
        {
            pseudolegals.push_back(
                Move(ourPawnSquare, enPassantSquare, MoveFlag::EnPassant)
            );
        });
    }

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshadow"

    // Pawns
    ITERATE_BITBOARD(ourPawns, fromSquare,
    {
        bool pawnHasntMoved = false;
        bool willPromote = false;

        const Rank rank = squareRank(fromSquare);
        assert(!isBackrank(rank));

        if (rank == Rank::Rank2) {
            pawnHasntMoved = stm == Color::White;
            willPromote    = stm == Color::Black;
        }
        else if (rank == Rank::Rank7) {
            pawnHasntMoved = stm == Color::Black;
            willPromote    = stm == Color::White;
        }

        if (moveGenType == MoveGenType::QuietOnly)
            goto pawnPushes;

        // Generate this pawn's captures

        ITERATE_BITBOARD(getPawnAttacks(fromSquare, stm) & pos.them(), toSquare,
        {
            if (willPromote)
                addPromotions(pseudolegals, fromSquare, toSquare, underpromos);
            else
                pseudolegals.push_back(Move(fromSquare, toSquare, MoveFlag::Pawn));
        });

        pawnPushes:

        const Square squareOneUp = add<Square>(
            fromSquare, stm == Color::White ? 8 : -8
        );

        if (pos.isOccupied(squareOneUp))
            continue;

        if (willPromote && moveGenType != MoveGenType::QuietOnly)
            addPromotions(pseudolegals, fromSquare, squareOneUp, underpromos);

        if (willPromote || moveGenType == MoveGenType::NoisyOnly)
            continue;

        // pawn single push
        pseudolegals.push_back(Move(fromSquare, squareOneUp, MoveFlag::Pawn));

        // pawn double push

        if (!pawnHasntMoved) continue;

        const Square squareTwoUp = add<Square>(
            fromSquare, stm == Color::White ? 16 : -16
        );

        if (!pos.isOccupied(squareTwoUp))
            pseudolegals.push_back(Move(fromSquare, squareTwoUp, MoveFlag::PawnDoublePush));
    });

    const Bitboard occ = pos.occupied();

    const Bitboard mask = moveGenType == MoveGenType::NoisyOnly ? pos.them()
                        : moveGenType == MoveGenType::QuietOnly ? ~pos.occupied()
                        : ~pos.us();

    // Knights
    ITERATE_BITBOARD(ourKnights, fromSquare,
    {
        ITERATE_BITBOARD(getKnightAttacks(fromSquare) & mask, toSquare,
        {
            pseudolegals.push_back(Move(fromSquare, toSquare, MoveFlag::Knight));
        });
    });

    // Bishops
    ITERATE_BITBOARD(ourBishops, fromSquare,
    {
        ITERATE_BITBOARD(getBishopAttacks(fromSquare, occ) & mask, toSquare,
        {
            pseudolegals.push_back(Move(fromSquare, toSquare, MoveFlag::Bishop));
        });
    });

    // Rooks
    ITERATE_BITBOARD(ourRooks, fromSquare,
    {
        ITERATE_BITBOARD(getRookAttacks(fromSquare, occ) & mask, toSquare,
        {
            pseudolegals.push_back(Move(fromSquare, toSquare, MoveFlag::Rook));
        });
    });

    // Queens
    ITERATE_BITBOARD(ourQueens, fromSquare,
    {
        ITERATE_BITBOARD(getQueenAttacks(fromSquare, occ) & mask, toSquare,
        {
            pseudolegals.push_back(Move(fromSquare, toSquare, MoveFlag::Queen));
        });
    });

    #pragma clang diagnostic pop // #pragma clang diagnostic ignored "-Wshadow"

    // King moves

    const Square kingSquare = pos.kingSquare();

    const Bitboard enemyAttacks = optEnemyAttacks
        ? *optEnemyAttacks
        : pos.attacks(pos.notSideToMove(), pos.occupied() ^ squareBb(kingSquare));

    const Bitboard kingMoves = getKingAttacks(kingSquare) & mask & ~enemyAttacks;

    ITERATE_BITBOARD(kingMoves, toSquare,
    {
        pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::King));
    });

    // If can't castle, return moves now
    if (moveGenType == MoveGenType::NoisyOnly || pos.inCheck())
        return pseudolegals;

    // Short castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][false])
    {
        const Square rookFrom = add<Square>(kingSquare, 3);

        const bool squaresFree = (occ & BETWEEN_EXCLUSIVE_BB[kingSquare][rookFrom]) == 0;

        const bool squaresNotThreatened
            = (enemyAttacks & BETWEEN_EXCLUSIVE_BB[kingSquare][rookFrom]) == 0;

        if (squaresFree && squaresNotThreatened)
        {
            const Square toSquare = add<Square>(kingSquare, 2);
            pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::Castling));
        }
    }

    // Long castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][true])
    {
        const Square rookFrom = add<Square>(kingSquare, -4);
        const Square nextToRook = next<Square>(rookFrom);

        const bool squaresFree = (occ & BETWEEN_EXCLUSIVE_BB[kingSquare][rookFrom]) == 0;

        const bool squaresNotThreatened
            = (enemyAttacks & BETWEEN_EXCLUSIVE_BB[kingSquare][nextToRook]) == 0;

        if (squaresFree && squaresNotThreatened)
        {
            const Square toSquare = add<Square>(kingSquare, -2);
            pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::Castling));
        }
    }

    return pseudolegals;
}

constexpr bool isPseudolegalLegal(Position& pos, const Move move)
{
    //assert(isPseudolegal(pos, move));

    if (move.pieceType() == PieceType::King)
        return true;

    // If 2 checkers, we must move king
    if (std::popcount(pos.checkers()) > 1)
        return false;

    const Square from = move.from();
    const Square to   = move.to();

    const Square kingSquare = pos.kingSquare();

    if (move.flag() == MoveFlag::EnPassant)
    {
        const Bitboard occAfter
            = pos.occupied() ^ squareBb(from) ^ squareBb(enPassantRelative(to)) ^ squareBb(to);

        const Bitboard bishopsQueens = pos.getBb(PieceType::Bishop) | pos.getBb(PieceType::Queen);
        const Bitboard rooksQueens   = pos.getBb(PieceType::Rook)   | pos.getBb(PieceType::Queen);

        Bitboard slidingAttackersTo = bishopsQueens & getBishopAttacks(kingSquare, occAfter);
        slidingAttackersTo         |= rooksQueens   & getRookAttacks(kingSquare, occAfter);

        return (slidingAttackersTo & pos.them()) == 0;
    }

    const Bitboard kingAndLineThru = squareBb(kingSquare) & LINE_THRU_BB[from][to];
    const Bitboard fromAndPinned = squareBb(from) & pos.pinned();

    if (kingAndLineThru == 0 && fromAndPinned > 0)
        return false;

    // 1 checker?
    if (pos.inCheck()) {
        const Square checkerSquare = lsb(pos.checkers());
        return squareBb(to) & (pos.checkers() | BETWEEN_EXCLUSIVE_BB[kingSquare][checkerSquare]);
    }

    return true;
}

constexpr u64 perft(Position& pos, const i32 depth)
{
    if (depth <= 0) return 1;

    const auto moves = pseudolegalMoves(pos, MoveGenType::AllMoves);
    u64 nodes = 0;

    for (const Move move : moves)
    {
        if (isPseudolegalLegal(pos, move))
        {
            if (depth == 1)
                nodes++;
            else {
                pos.makeMove(move);
                nodes += perft(pos, depth - 1);
                pos.undoMove();
            }
        }
    }

    return nodes;
}

constexpr u64 perftSplit(Position& pos, const i32 depth)
{
    if (depth <= 0) {
        std::cout << "Total: 1" << std::endl;
        return 1;
    }

    const auto moves = pseudolegalMoves(pos, MoveGenType::AllMoves);
    u64 totalNodes = 0;

    for (const Move move : moves)
        if (isPseudolegalLegal(pos, move))
        {
            pos.makeMove(move);
            const u64 nodes = perft(pos, depth - 1);
            std::cout << move.toUci() << ": " << nodes << std::endl;
            totalNodes += nodes;
            pos.undoMove();
        }

    std::cout << "Total: " << totalNodes << std::endl;
    return totalNodes;
}
