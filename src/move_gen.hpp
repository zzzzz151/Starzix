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
    ArrayVec<Move, 256>& moves, const Square from, const Square to)
{
    moves.push_back(Move(from, to, MoveFlag::QueenPromo));
    moves.push_back(Move(from, to, MoveFlag::KnightPromo));
    moves.push_back(Move(from, to, MoveFlag::RookPromo));
    moves.push_back(Move(from, to, MoveFlag::BishopPromo));
}

constexpr ArrayVec<Move, 256> pseudolegalMoves(
    Position& pos, const MoveGenType moveGenType)
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
                addPromotions(pseudolegals, fromSquare, toSquare);
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
            addPromotions(pseudolegals, fromSquare, squareOneUp);

        if (willPromote || moveGenType == MoveGenType::NoisyOnly)
            continue;

        // Pawn single push
        pseudolegals.push_back(Move(fromSquare, squareOneUp, MoveFlag::Pawn));

        // Pawn double push

        if (!pawnHasntMoved) continue;

        const Square squareTwoUp = add<Square>(
            fromSquare, stm == Color::White ? 16 : -16
        );

        if (!pos.isOccupied(squareTwoUp))
            pseudolegals.push_back(Move(fromSquare, squareTwoUp, MoveFlag::PawnDoublePush));
    });

    const Bitboard occ = pos.occupied();

    const Bitboard mask = moveGenType == MoveGenType::NoisyOnly ? pos.them()
                        : moveGenType == MoveGenType::QuietOnly ? ~occ
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
    const Bitboard enemyAttacks = pos.enemyAttacksNoStmKing();
    const Bitboard kingMoves = getKingAttacks(kingSquare) & mask & ~enemyAttacks;

    ITERATE_BITBOARD(kingMoves, toSquare,
    {
        pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::King));
    });

    // If can't castle, return moves now
    if (moveGenType == MoveGenType::NoisyOnly || pos.inCheck())
        goto end;

    // Short castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][false])
    {
        const Bitboard squareThruAndDst
            = squareBb(add<Square>(kingSquare, 1)) | squareBb(add<Square>(kingSquare, 2));

        if ((squareThruAndDst & (occ | enemyAttacks)) == 0)
        {
            const Square toSquare = add<Square>(kingSquare, 2);
            pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::Castling));
        }
    }

    // Long castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][true])
    {
        const Bitboard squareThruAndDst
            = squareBb(add<Square>(kingSquare, -1)) | squareBb(add<Square>(kingSquare, -2));

        const Bitboard mustBeEmpty = squareThruAndDst | squareBb(add<Square>(kingSquare, -3));

        if ((mustBeEmpty & occ) == 0 && (squareThruAndDst & enemyAttacks) == 0)
        {
            const Square toSquare = add<Square>(kingSquare, -2);
            pseudolegals.push_back(Move(kingSquare, toSquare, MoveFlag::Castling));
        }
    }

    end:

    assert([&] () {
        if (moveGenType == MoveGenType::AllMoves)
            return true;

        for (const Move move : pseudolegals)
            assert(pos.isQuiet(move) == (moveGenType == MoveGenType::QuietOnly));

        return true;
    }());

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

constexpr bool hasLegalMove(Position& pos)
{
    const Color stm = pos.sideToMove();
    const Bitboard occ = pos.occupied();

    // Does our king have legal move?

    const Square kingSquare = pos.kingSquare();
    const Bitboard enemyAttacks = pos.enemyAttacksNoStmKing();
    const Bitboard targetSquares = getKingAttacks(kingSquare) & ~pos.us() & ~enemyAttacks;

    if (targetSquares > 0) return true;

    const auto numCheckers = std::popcount(pos.checkers());
    assert(numCheckers <= 2);

    // If in double check, only king moves are allowed
    if (numCheckers > 1) return false;

    Bitboard movableBb = ~0ULL;

    if (numCheckers == 1)
    {
        movableBb = pos.checkers();

        const Bitboard sliders = pos.getBb(PieceType::Bishop)
                               | pos.getBb(PieceType::Rook)
                               | pos.getBb(PieceType::Queen);

        if (pos.checkers() & sliders)
        {
            const Square checkerSquare = lsb(pos.checkers());
            movableBb |= BETWEEN_EXCLUSIVE_BB[kingSquare][checkerSquare];
        }
    }

    if (pos.inCheck()) goto castlingDone;

    // Short castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][false])
    {
        const Bitboard squareThruAndDst
            = squareBb(add<Square>(kingSquare, 1)) | squareBb(add<Square>(kingSquare, 2));

        if ((squareThruAndDst & (occ | enemyAttacks)) == 0)
            return true;
    }

    // Long castling
    if (pos.castlingRights() & CASTLING_MASKS[stm][true])
    {
        const Bitboard squareThruAndDst
            = squareBb(add<Square>(kingSquare, -1)) | squareBb(add<Square>(kingSquare, -2));

        const Bitboard mustBeEmpty = squareThruAndDst | squareBb(add<Square>(kingSquare, -3));

        if ((mustBeEmpty & occ) == 0 && (squareThruAndDst & enemyAttacks) == 0)
            return true;
    }

    castlingDone:

    // Calculate pinnedOrthogonal

    Bitboard pinnedOrthogonal = 0;

    const Bitboard rookAttacks = getRookAttacks(kingSquare, occ);

    const Bitboard xrayRook
        = rookAttacks ^ getRookAttacks(kingSquare, occ ^ (pos.us() & rookAttacks));

    Bitboard pinnersOrthogonal = pos.getBb(PieceType::Rook) | pos.getBb(PieceType::Queen);
    pinnersOrthogonal &= xrayRook & pos.them();

    ITERATE_BITBOARD(pinnersOrthogonal, pinnerSquare,
    {
        pinnedOrthogonal |= pos.us() & BETWEEN_EXCLUSIVE_BB[kingSquare][pinnerSquare];
    });

    // Calculate pinnedDiagonal

    Bitboard pinnedDiagonal = 0;

    const Bitboard bishopAttacks = getBishopAttacks(kingSquare, occ);

    const Bitboard xrayBishop
        = bishopAttacks ^ getBishopAttacks(kingSquare, occ ^ (pos.us() & bishopAttacks));

    Bitboard pinnersDiagonal = pos.getBb(PieceType::Bishop) | pos.getBb(PieceType::Queen);
    pinnersDiagonal &= xrayBishop & pos.them();

    ITERATE_BITBOARD(pinnersDiagonal, pinnerSquare,
    {
        pinnedDiagonal |= pos.us() & BETWEEN_EXCLUSIVE_BB[kingSquare][pinnerSquare];
    });

    // Check if our non-king pieces have a legal move

    const Bitboard ourKnights
        = pos.getBb(stm, PieceType::Knight) & ~pinnedDiagonal & ~pinnedOrthogonal;

    ITERATE_BITBOARD(ourKnights, fromSquare,
    {
        if (getKnightAttacks(fromSquare) & ~pos.us() & movableBb)
            return true;
    });

    const Bitboard ourBishops = pos.getBb(stm, PieceType::Bishop) & ~pinnedOrthogonal;

    ITERATE_BITBOARD(ourBishops, fromSquare,
    {
        Bitboard bishopMoves = getBishopAttacks(fromSquare, occ) & ~pos.us() & movableBb;

        if (squareBb(fromSquare) & pinnedDiagonal)
            bishopMoves &= LINE_THRU_BB[kingSquare][fromSquare];

        if (bishopMoves > 0) return true;
    });

    const Bitboard ourRooks = pos.getBb(stm, PieceType::Rook) & ~pinnedDiagonal;

    ITERATE_BITBOARD(ourRooks, fromSquare,
    {
        Bitboard rookMoves = getRookAttacks(fromSquare, occ) & ~pos.us() & movableBb;

        if (squareBb(fromSquare) & pinnedOrthogonal)
            rookMoves &= LINE_THRU_BB[kingSquare][fromSquare];

        if (rookMoves > 0) return true;
    });

    ITERATE_BITBOARD(pos.getBb(stm, PieceType::Queen), fromSquare,
    {
        Bitboard queenMoves = getQueenAttacks(fromSquare, occ) & ~pos.us() & movableBb;

        if (squareBb(fromSquare) & (pinnedDiagonal | pinnedOrthogonal))
            queenMoves &= LINE_THRU_BB[kingSquare][fromSquare];

        if (queenMoves > 0) return true;
    });

    // En passant
    if (pos.enPassantSquare())
    {
        const Square enPassantSquare = *(pos.enPassantSquare());
        const Bitboard enPassantSqBb = squareBb(enPassantSquare);

        const Bitboard ourNearbyPawns = pos.getBb(stm, PieceType::Pawn)
                                      & getPawnAttacks(enPassantSquare, pos.notSideToMove());

        ITERATE_BITBOARD(ourNearbyPawns, ourPawnSquare,
        {
            auto colorBbs  = pos.colorBbs();
            auto piecesBbs = pos.piecesBbs();

            const Bitboard ourPawnBb = squareBb(ourPawnSquare);
            const Bitboard capturedPawnBb = squareBb(enPassantRelative(enPassantSquare));

            // Make en passant move

            colorBbs[stm] ^= ourPawnBb;
            colorBbs[stm] ^= enPassantSqBb;
            colorBbs[pos.notSideToMove()] ^= capturedPawnBb;

            piecesBbs[PieceType::Pawn] ^= ourPawnBb;
            piecesBbs[PieceType::Pawn] ^= enPassantSqBb;
            piecesBbs[PieceType::Pawn] ^= capturedPawnBb;

            // If after the en passant our king is under attack, then en passant was illegal

            if (pos.getBb(pos.notSideToMove(), PieceType::Pawn) & getPawnAttacks(kingSquare, stm))
                return true;

            if (pos.getBb(pos.notSideToMove(), PieceType::Knight) & getKnightAttacks(kingSquare))
                return true;

            const Bitboard occAfter = colorBbs[Color::White] | colorBbs[Color::Black];

            const Bitboard enemyBishopsQueens
                = pos.them() & (pos.getBb(PieceType::Bishop) | pos.getBb(PieceType::Queen));

            if (enemyBishopsQueens & getBishopAttacks(kingSquare, occAfter))
                return true;

            const Bitboard enemyRooksQueens
                = pos.them() & (pos.getBb(PieceType::Rook) | pos.getBb(PieceType::Queen));

            if (enemyRooksQueens & getRookAttacks(kingSquare, occAfter))
                return true;
        });
    }

    ITERATE_BITBOARD(pos.getBb(stm, PieceType::Pawn), fromSquare,
    {
        // Pawn's captures

        Bitboard pawnAttacks = getPawnAttacks(fromSquare, stm) & pos.them() & movableBb;

        if (squareBb(fromSquare) & (pinnedDiagonal | pinnedOrthogonal))
            pawnAttacks &= LINE_THRU_BB[kingSquare][fromSquare];

        if (pawnAttacks > 0) return true;

        // Pawn single push

        if (squareBb(fromSquare) & pinnedDiagonal)
            continue;

        const Bitboard pinRay = LINE_THRU_BB[fromSquare][kingSquare];

        const bool pinnedHorizontally
            = (squareBb(fromSquare) & pinnedOrthogonal) > 0 && (pinRay & (pinRay << 1)) > 0;

        if (pinnedHorizontally) continue;

        const Square squareOneUp = add<Square>(fromSquare, stm == Color::White ? 8 : -8);

        if (pos.isOccupied(squareOneUp))
            continue;

        if (squareBb(squareOneUp) & movableBb)
            return true;

        // Pawn double push
        if (squareRank(fromSquare) == (stm == Color::White ? Rank::Rank2 : Rank::Rank7))
        {
            const Square squareTwoUp = add<Square>(fromSquare, stm == Color::White ? 16 : -16);

            if ((squareBb(squareTwoUp) & movableBb) && !pos.isOccupied(squareTwoUp))
                return true;
        }
    });

    return false;
}

constexpr u64 perft(Position& pos, const i32 depth)
{
    if (depth <= 0) return 1;

    const auto moves = pseudolegalMoves(pos, MoveGenType::AllMoves);
    u64 nodes = 0;

    for (const Move move : moves)
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

    assert(hasLegalMove(pos) == (nodes != 0));

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
