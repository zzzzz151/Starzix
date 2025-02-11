// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"

enum class GameState : i32 {
    Draw, Loss, Ongoing
};

// [color][isLongCastle]
constexpr EnumArray<std::array<Square, 2>, Color> CASTLING_ROOK_FROM =
{
    // White short and long castle
    std::array<Square, 2> { Square::H1, Square::A1 },
    // Black short and long castle
    std::array<Square, 2> { Square::H8, Square::A8 }
};

// [kingTargetSquare]
constexpr EnumArray<std::pair<Square, Square>, Square> CASTLING_ROOK_FROM_TO = [] () consteval
{
    EnumArray<std::pair<Square, Square>, Square> castlingRookFromTo = { };

    castlingRookFromTo[Square::G1] = { Square::H1, Square::F1 }; // White short castle
    castlingRookFromTo[Square::C1] = { Square::A1, Square::D1 }; // White long castle
    castlingRookFromTo[Square::G8] = { Square::H8, Square::F8 }; // Black short castle
    castlingRookFromTo[Square::C8] = { Square::A8, Square::D8 }; // Black long castle

    return castlingRookFromTo;
}();

struct PosState
{
public:
    Color colorToMove = Color::White;
    EnumArray<Bitboard, Color>     colorBbs  = { }; // [color]
    EnumArray<Bitboard, PieceType> piecesBbs = { }; // [pieceType]
    Bitboard castlingRights = 0;
    std::optional<Square> enPassantSquare = std::nullopt;
    u8 pliesSincePawnOrCapture = 0;
    u16 currentMoveCounter = 1;
    u16 lastMove = MOVE_NONE.asU16();
    std::optional<PieceType> captured = std::nullopt;
    Bitboard checkers = 0;
    std::optional<Bitboard> pinned = std::nullopt;
    std::optional<Bitboard> enemyAttacksNoStmKing = std::nullopt;
    u64 zobristHash = 0;
    u64 pawnsHash = 0;
    EnumArray<u64, Color> nonPawnsHashes = { }; // [pieceColor]
}; // struct PosState

class Position
{
private:
    std::vector<PosState> mStates = { };

    constexpr const PosState& state() const
    {
        assert(!mStates.empty());
        return mStates.back();
    }

    constexpr PosState& state()
    {
        assert(!mStates.empty());
        return mStates.back();
    }

public:

    constexpr Position() = default;

    inline Position(const std::string& fen)
    {
        mStates = { };
        mStates.reserve(512);
        mStates.push_back(PosState());

        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move

        state().colorToMove = fenSplit[1] == "b" ? Color::Black : Color::White;

        if (state().colorToMove == Color::Black)
            state().zobristHash ^= ZOBRIST_COLOR;

        // Parse pieces
        // Iterate ranks from top to bottom, files from left to right

        const std::string fenRows = fenSplit[0];

        Rank currentRank = Rank::Rank8;
        File currentFile = File::A;

        for (char thisChar : fenRows)
        {
            if (thisChar == '/')
            {
                currentRank = previous<Rank>(currentRank);
                currentFile = File::A;
            }
            else if (isdigit(thisChar))
                currentFile = addAndClamp<File>(currentFile, charToI32(thisChar));
            else
            {
                const Square square = toSquare(currentFile, currentRank);
                const Color color = isupper(thisChar) ? Color::White : Color::Black;

                thisChar = static_cast<char>(std::tolower(thisChar));

                const PieceType pt = thisChar == 'p' ? PieceType::Pawn
                                   : thisChar == 'n' ? PieceType::Knight
                                   : thisChar == 'b' ? PieceType::Bishop
                                   : thisChar == 'r' ? PieceType::Rook
                                   : thisChar == 'q' ? PieceType::Queen
                                   : PieceType::King;

                placePiece(color, pt, square);

                currentFile = addAndClamp<File>(currentFile, 1);
            }
        }

        // Parse castling rights

        const std::string fenCastlingRights = fenSplit[2];

        if (fenCastlingRights != "-")
        {
            for (const char thisChar : fenCastlingRights)
            {
                const Color color = isupper(thisChar) ? Color::White : Color::Black;
                const bool isLongCastle = thisChar == 'Q' || thisChar == 'q';

                state().castlingRights |= squareBb(CASTLING_ROOK_FROM[color][isLongCastle]);
            }

            state().zobristHash ^= state().castlingRights;
        }

        // Parse en passant target square

        const std::string strEnPassantSquare = fenSplit[3];

        if (strEnPassantSquare != "-")
        {
            const Square enPassantSquare = strToSquare(strEnPassantSquare);
            state().enPassantSquare = enPassantSquare;
            state().zobristHash ^= ZOBRIST_FILES[squareFile(enPassantSquare)];
        }

        // Parse last 2 fen tokens

        if (fenSplit.size() >= 5)
            state().pliesSincePawnOrCapture = static_cast<u8>(stoi(fenSplit[4]));

        if (fenSplit.size() >= 6)
            state().currentMoveCounter = static_cast<u16>(stoi(fenSplit[5]));

        state().checkers = attackers(kingSquare()) & them();
    }

    constexpr size_t numStates() const
    {
        return mStates.size();
    }

    constexpr Color sideToMove() const
    {
        return state().colorToMove;
    }

    constexpr Color notSideToMove() const
    {
        return oppColor(sideToMove());
    }

    constexpr auto colorBbs() const {
        return state().colorBbs;
    }

    constexpr auto piecesBbs() const {
        return state().piecesBbs;
    }

    constexpr Bitboard getBb(const Color color) const
    {
        return state().colorBbs[color];
    }

    constexpr Bitboard getBb(const PieceType pieceType) const
    {
        return state().piecesBbs[pieceType];
    }

    constexpr Bitboard getBb(const Color color, const PieceType pieceType) const
    {
        return getBb(color) & getBb(pieceType);
    }

    constexpr Bitboard us() const {
        return getBb(sideToMove());
    }

    constexpr Bitboard them() const {
        return getBb(notSideToMove());
    }

    constexpr Bitboard occupied() const {
        return getBb(Color::White) | getBb(Color::Black);
    }

    constexpr bool isOccupied(const Square square) const
    {
        return hasSquare(occupied(), square);
    }

    constexpr auto castlingRights() const
    {
        return state().castlingRights;
    }

    constexpr std::optional<Square> enPassantSquare() const
    {
        return state().enPassantSquare;
    }

    constexpr void setPliesSincePawnOrCapture(const u8 newValue)
    {
        state().pliesSincePawnOrCapture = newValue;
    }

    constexpr Move lastMove() const {
        return state().lastMove;
    }

    constexpr Move nthToLastMove(const size_t n) const
    {
        assert(n >= 1);

        return n > mStates.size()
             ? MOVE_NONE
             : Move(mStates[mStates.size() - n].lastMove);
    }

    constexpr auto captured() const {
        return state().captured;
    }

    constexpr Bitboard checkers() const {
        return state().checkers;
    }

    constexpr bool inCheck() const {
        return state().checkers > 0;
    }

    constexpr u64 zobristHash() const {
        return state().zobristHash;
    }

    constexpr u64 pawnsHash() const {
        return state().pawnsHash;
    }

    constexpr u64 nonPawnsHash(const Color color) const
    {
        return state().nonPawnsHashes[color];
    }

    constexpr std::optional<Color> colorAt(const Square square) const
    {
        if (!isOccupied(square))
            return std::nullopt;

        return hasSquare(getBb(Color::White), square)
             ? Color::White : Color::Black;
    }

    constexpr std::optional<PieceType> pieceTypeAt(const Square square) const
    {
        if (!isOccupied(square))
            return std::nullopt;

        for (const PieceType pt : EnumIter<PieceType>())
            if (hasSquare(getBb(pt), square))
                return pt;

        return std::nullopt;
    }

    constexpr Square kingSquare(const Color color) const
    {
        return lsb(getBb(color, PieceType::King));
    }

    constexpr Square kingSquare() const
    {
        return kingSquare(sideToMove());
    }

private:

    constexpr void placePiece(
        const Color color, const PieceType pieceType, const Square square)
    {
        assert(!isOccupied(square));

        state().colorBbs[color]      |= squareBb(square);
        state().piecesBbs[pieceType] |= squareBb(square);

        updateHashes(color, pieceType, square);
    }

    constexpr void removePiece(
        const Color color, const PieceType pieceType, const Square square)
    {
        assert(hasSquare(getBb(color, pieceType), square));

        state().colorBbs[color]      ^= squareBb(square);
        state().piecesBbs[pieceType] ^= squareBb(square);

        updateHashes(color, pieceType, square);
    }

    constexpr void updateHashes(
        const Color color, const PieceType pieceType, const Square square)
    {
        state().zobristHash ^= ZOBRIST_PIECES[color][pieceType][square];

        if (pieceType == PieceType::Pawn)
            state().pawnsHash ^= ZOBRIST_PIECES[color][pieceType][square];
        else
            state().nonPawnsHashes[color] ^= ZOBRIST_PIECES[color][pieceType][square];
    }

public:

    inline std::string fen() const
    {
        std::string myFen = "";

        Rank rank = Rank::Count;

        while (rank != Rank::Rank1)
        {
            rank = previous<Rank>(rank);
            size_t emptySoFar = 0;

            for (const File file : EnumIter<File>())
            {
                const Square square = toSquare(file, rank);
                const std::optional<PieceType> pt = pieceTypeAt(square);

                if (pt && emptySoFar > 0)
                    myFen += std::to_string(emptySoFar);

                if (pt) {
                    myFen += letterOf(*colorAt(square), *pt);
                    emptySoFar = 0;
                }
                else
                    emptySoFar++;

                if (file == File::H && emptySoFar > 0)
                    myFen += std::to_string(emptySoFar);
            }

            myFen += "/";
        }

        myFen.pop_back(); // Remove last '/'

        myFen += " ";
        myFen += sideToMove() == Color::White ? "w" : "b";

        myFen += " ";

        if (hasSquare(state().castlingRights, CASTLING_ROOK_FROM[Color::White][false]))
            myFen += "K";

        if (hasSquare(state().castlingRights, CASTLING_ROOK_FROM[Color::White][true]))
            myFen += "Q";

        if (hasSquare(state().castlingRights, CASTLING_ROOK_FROM[Color::Black][false]))
            myFen += "k";

        if (hasSquare(state().castlingRights, CASTLING_ROOK_FROM[Color::Black][true]))
            myFen += "q";

        // No castling rights
        if (myFen.back() == ' ')
            myFen += "-";

        myFen += " ";

        myFen += state().enPassantSquare
               ? squareToStr(*(state().enPassantSquare))
               : "-";

        myFen += " " + std::to_string(state().pliesSincePawnOrCapture);
        myFen += " " + std::to_string(state().currentMoveCounter);

        return myFen;
    }

    inline void print() const
    {
        Rank rank = Rank::Count;

        while (rank != Rank::Rank1)
        {
            rank = previous<Rank>(rank);

            for (const File file : EnumIter<File>())
            {
                const Square square = toSquare(file, rank);
                const std::optional<PieceType> pt = pieceTypeAt(square);

                std::cout << (pt ? letterOf(*colorAt(square), *pt) : '.')
                          << (file == File::H ? '|' : ' ');
            }

            std::cout << (static_cast<i32>(rank) + 1)
                      << '\n';
        }

        std::cout << std::string(15, '-') // 15 dashes
                  << "+\nA B C D E F G H";

        std::cout << "\n\n"
                  << fen()
                  << "\nZobrist hash: "
                  << state().zobristHash
                  << "\nMoves:";

        for (size_t i = 1; i < mStates.size(); i++)
            std::cout << " " << Move(mStates[i].lastMove).toUci();

        std::cout << std::endl; // Flush
    }

    constexpr std::optional<PieceType> captured(const Move move) const
    {
        assert(move != MOVE_NONE);

        return move.flag() == MoveFlag::EnPassant
             ? PieceType::Pawn
             : pieceTypeAt(move.to());
    }

    constexpr bool isQuiet(const Move move) const
    {
        assert(move != MOVE_NONE);

        return !isOccupied(move.to())
            && move.flag() != MoveFlag::EnPassant
            && !move.promotion();
    }

    constexpr GameState gameState(
        bool (*fnHasLegalMove) (Position&),
        const std::optional<size_t> searchPly = std::nullopt)
    {
        if (!fnHasLegalMove(*this))
            return inCheck() ? GameState::Loss : GameState::Draw;

        if (state().pliesSincePawnOrCapture >= 100)
            return GameState::Draw;

        const auto numPieces = std::popcount(occupied());

        // K vs K
        if (numPieces == 2)
            return GameState::Draw;
        // KN vs K
        // KB vs K
        else if (numPieces == 3)
        {
            if (getBb(PieceType::Knight) > 0 || getBb(PieceType::Bishop) > 0)
                return GameState::Draw;
        }
        // KNN vs K
        // KN vs KN
        // KN vs KB
        // KB vs KB
        else if (numPieces == 4)
        {
            const auto numWhiteKnights = std::popcount(getBb(Color::White, PieceType::Knight));
            const auto numBlackKnights = std::popcount(getBb(Color::Black, PieceType::Knight));

            if (numWhiteKnights + numBlackKnights == 2)
                return GameState::Draw;

            const auto numWhiteBishops = std::popcount(getBb(Color::White, PieceType::Bishop));
            const auto numBlackBishops = std::popcount(getBb(Color::Black, PieceType::Bishop));

            if ((numWhiteKnights == 1 && numBlackBishops == 1)
            ||  (numBlackKnights == 1 && numWhiteBishops == 1)
            ||  (numWhiteBishops == 1 && numBlackBishops == 1))
                return GameState::Draw;
        }

        // Repetition detection

        if (mStates.size() <= 4 || state().pliesSincePawnOrCapture < 4)
            return GameState::Ongoing;

        const i32 numStates = static_cast<i32>(mStates.size());
        const i32 pliesSincePawnOrCapt = static_cast<i32>(state().pliesSincePawnOrCapture);

        const i32 stateIdxAfterPawnOrCapture
            = std::max<i32>(0, numStates - pliesSincePawnOrCapt - 1);

        const i32 rootStateIdx
            = searchPly ? numStates - static_cast<i32>(*searchPly) - 1 : -1;

        size_t count = 0;

        for (i32 i = numStates - 3; i >= stateIdxAfterPawnOrCapture; i -= 2)
            if (mStates[static_cast<size_t>(i)].zobristHash == state().zobristHash
            && (i > rootStateIdx || ++count == 2))
                return GameState::Draw;

        return GameState::Ongoing;
    }

    constexpr bool stmHasNonPawns() const
    {
        return getBb(sideToMove(), PieceType::Knight) > 0
            || getBb(sideToMove(), PieceType::Bishop) > 0
            || getBb(sideToMove(), PieceType::Rook)   > 0
            || getBb(sideToMove(), PieceType::Queen)  > 0;
    }

    constexpr Bitboard pinned()
    {
        // Cached?
        if (state().pinned) return *(state().pinned);

        const Square kingSquare = this->kingSquare();

        const Bitboard theirBishopsQueens
            = them() & (getBb(PieceType::Bishop) | getBb(PieceType::Queen));

        const Bitboard theirRooksQueens
            = them() & (getBb(PieceType::Rook)   | getBb(PieceType::Queen));

        Bitboard potentialAttackers
            = theirBishopsQueens & getBishopAttacks(kingSquare, them());

        potentialAttackers
            |= theirRooksQueens   & getRookAttacks(kingSquare, them());

        state().pinned = 0;

        ITERATE_BITBOARD(potentialAttackers, attackerSquare,
        {
            const Bitboard maybePinned
                = us() & BETWEEN_EXCLUSIVE_BB[kingSquare][attackerSquare];

            if (std::popcount(maybePinned) == 1)
                *(state().pinned) |= maybePinned;
        });

        return *(state().pinned);
    }

    constexpr Bitboard attacks(const Color color, const Bitboard occ) const
    {
        Bitboard attacksBb = 0;

        ITERATE_BITBOARD(getBb(color, PieceType::Pawn), square,
        {
            attacksBb |= getPawnAttacks(square, color);
        });

        ITERATE_BITBOARD(getBb(color, PieceType::Knight), square,
        {
            attacksBb |= getKnightAttacks(square);
        });

        const Bitboard bishopsQueens
            = getBb(color) & (getBb(PieceType::Bishop) | getBb(PieceType::Queen));

        ITERATE_BITBOARD(bishopsQueens, square,
        {
            attacksBb |= getBishopAttacks(square, occ);
        });

        const Bitboard rooksQueens
            = getBb(color) & (getBb(PieceType::Rook) | getBb(PieceType::Queen));

        ITERATE_BITBOARD(rooksQueens, square,
        {
            attacksBb |= getRookAttacks(square, occ);
        });

        attacksBb |= getKingAttacks(kingSquare(color));

        assert(attacksBb > 0);
        return attacksBb;
    }

    constexpr Bitboard attacks(const Color color) const
    {
        return attacks(color, occupied());
    }

    constexpr Bitboard enemyAttacksNoStmKing()
    {
        // If cached, return it
        if (state().enemyAttacksNoStmKing)
            return *(state().enemyAttacksNoStmKing);

        // Calculate and cache
        const Bitboard occNoStmKing = occupied() ^ squareBb(kingSquare());
        state().enemyAttacksNoStmKing = attacks(notSideToMove(), occNoStmKing);

        return *(state().enemyAttacksNoStmKing);
    }

    constexpr Bitboard attackers(const Square square, const Bitboard occ) const
    {
        const Bitboard bishopsQueens = getBb(PieceType::Bishop) | getBb(PieceType::Queen);
        const Bitboard rooksQueens   = getBb(PieceType::Rook)   | getBb(PieceType::Queen);

        Bitboard attackers = getBb(Color::Black, PieceType::Pawn)
                           & getPawnAttacks(square, Color::White);

        attackers |= getBb(Color::White, PieceType::Pawn)
                   & getPawnAttacks(square, Color::Black);

        attackers |= getBb(PieceType::Knight) & getKnightAttacks(square);

        attackers |= bishopsQueens & getBishopAttacks(square, occ);
        attackers |= rooksQueens   & getRookAttacks(square, occ);

        attackers |= getBb(PieceType::King) & getKingAttacks(square);

        return attackers;
    }

    constexpr Bitboard attackers(const Square square) const
    {
        return attackers(square, occupied());
    }

    constexpr void makeMove(const Move move)
    {
        const PosState copy = state();
        mStates.push_back(copy);

        state().lastMove = move.asU16();

        // These will be calculated and cached later
        state().pinned = std::nullopt;
        state().enemyAttacksNoStmKing = std::nullopt;

        if (move == MOVE_NONE) {
            assert(!inCheck());

            state().colorToMove = notSideToMove();
            state().zobristHash ^= ZOBRIST_COLOR;

            if (state().enPassantSquare)
            {
                const Square enPassantSquare = *(state().enPassantSquare);
                state().enPassantSquare = std::nullopt;
                state().zobristHash ^= ZOBRIST_FILES[squareFile(enPassantSquare)];
            }

            state().pliesSincePawnOrCapture++;

            if (sideToMove() == Color::White)
                state().currentMoveCounter++;

            state().captured = std::nullopt;
            return;
        }

        const Square from = move.from();
        const Square to   = move.to();
        const MoveFlag moveFlag = move.flag();
        const std::optional<PieceType> promotion = move.promotion();
        const PieceType pieceType = move.pieceType();

        removePiece(sideToMove(), pieceType, from);

        if (moveFlag == MoveFlag::Castling)
        {
            placePiece(sideToMove(), PieceType::King, to);

            const auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];

            removePiece(sideToMove(), PieceType::Rook, rookFrom);
            placePiece(sideToMove(), PieceType::Rook, rookTo);

            state().captured = std::nullopt;
        }
        else if (moveFlag == MoveFlag::EnPassant)
        {
            removePiece(notSideToMove(), PieceType::Pawn, enPassantRelative(to));
            placePiece(sideToMove(), PieceType::Pawn, to);

            state().captured = PieceType::Pawn;
        }
        else {
            state().captured = pieceTypeAt(to);

            if (state().captured)
                removePiece(notSideToMove(), *(state().captured), to);

            placePiece(sideToMove(), promotion ? *promotion : pieceType, to);
        }

        state().zobristHash ^= state().castlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::King)
        {
            state().castlingRights &= ~squareBb(CASTLING_ROOK_FROM[sideToMove()][false]);
            state().castlingRights &= ~squareBb(CASTLING_ROOK_FROM[sideToMove()][true]);
        }
        else if (hasSquare(state().castlingRights, from))
            state().castlingRights &= ~squareBb(from);
        else if (hasSquare(state().castlingRights, to))
            state().castlingRights &= ~squareBb(to);

        state().zobristHash ^= state().castlingRights; // XOR new castling rights in

        // Remove old en passant square if one exists
        if (state().enPassantSquare)
        {
            const Square enPassantSquare = *(state().enPassantSquare);
            state().enPassantSquare = std::nullopt;
            state().zobristHash ^= ZOBRIST_FILES[squareFile(enPassantSquare)];
        }

        // If pawn double push, create new en passant square
        if (moveFlag == MoveFlag::PawnDoublePush)
        {
            const Square enPassantSquare = enPassantRelative(to);
            state().enPassantSquare = enPassantSquare;
            state().zobristHash ^= ZOBRIST_FILES[squareFile(enPassantSquare)];
        }

        state().colorToMove = notSideToMove();
        state().zobristHash ^= ZOBRIST_COLOR;

        if (pieceType == PieceType::Pawn || state().captured)
            state().pliesSincePawnOrCapture = 0;
        else
            state().pliesSincePawnOrCapture++;

        if (sideToMove() == Color::White)
            state().currentMoveCounter++;

        state().checkers = attackers(kingSquare()) & them();
    }

    constexpr void undoMove()
    {
        assert(mStates.size() > 1);
        mStates.pop_back();
    }

    inline Move uciToMove(const std::string uciMove) const
    {
        const Square from = strToSquare(uciMove.substr(0, 2));
        const Square to   = strToSquare(uciMove.substr(2, 2));

        const std::optional<PieceType> pt = pieceTypeAt(from);

        if (!pt) throw std::logic_error("No piece in this square to move");

        // Promotion?
        if (uciMove.size() == 5)
        {
            // Last char of string uciMove
            const char promotion = static_cast<char>(std::tolower(uciMove.back()));

            if (promotion == 'n')
                return Move(from, to, MoveFlag::KnightPromo);
            else if (promotion == 'b')
                return Move(from, to, MoveFlag::BishopPromo);
            else if (promotion == 'r')
                return Move(from, to, MoveFlag::RookPromo);
            else
                return Move(from, to, MoveFlag::QueenPromo);
        }

        MoveFlag moveFlag = asEnum<MoveFlag>(static_cast<i32>(*pt) + 1);
        const i32 squaresTraveled = std::abs(static_cast<i32>(to) - static_cast<i32>(from));

        if (*pt == PieceType::King && squaresTraveled == 2)
            moveFlag = MoveFlag::Castling;
        else if (*pt == PieceType::Pawn)
        {
            if (isBackrank(squareRank(to)))
                moveFlag = MoveFlag::QueenPromo;
            else if (squaresTraveled == 16)
                moveFlag = MoveFlag::PawnDoublePush;
            else if (squaresTraveled != 8 && !isOccupied(to))
                moveFlag = MoveFlag::EnPassant;
        }

        return Move(from, to, moveFlag);
    }

    inline void makeMove(const std::string uciMove)
    {
        makeMove(uciToMove(uciMove));
    }

    // Static Exchange Evaluation
    constexpr bool SEE(const Move move, const i32 threshold = 0) const
    {
        assert(move != MOVE_NONE);

        constexpr EnumArray<i32, PieceType> SEE_PIECE_VALUES = { 100, 300, 300, 500, 900, 0 };

        i32 score = -threshold;

        const std::optional<PieceType> captured = this->captured(move);

        if (captured)
            score += SEE_PIECE_VALUES[*captured];

        const std::optional<PieceType> promotion = move.promotion();

        if (promotion)
            score += SEE_PIECE_VALUES[*promotion] - SEE_PIECE_VALUES[PieceType::Pawn];

        if (score < 0) return false;

        const PieceType next = promotion ? *promotion : move.pieceType();
        score -= SEE_PIECE_VALUES[next];

        if (score >= 0) return true;

        const Square from = move.from();
        const Square to   = move.to();

        const Bitboard bishopsQueens = getBb(PieceType::Bishop) | getBb(PieceType::Queen);
        const Bitboard rooksQueens   = getBb(PieceType::Rook)   | getBb(PieceType::Queen);

        Bitboard occ = occupied() ^ squareBb(from) ^ squareBb(to);
        Bitboard attackers = this->attackers(to, occ);

        Color ourColor = notSideToMove();

        const auto popLeastValuable = [&] (const Bitboard ourAttackers) constexpr
            -> std::optional<PieceType>
        {
            for (const PieceType pt : EnumIter<PieceType>())
            {
                const Bitboard bb = ourAttackers & getBb(ourColor, pt);

                if (bb > 0) {
                    const Square square = lsb(bb);
                    occ ^= squareBb(square);
                    return pt;
                }
            }

            return std::nullopt;
        };

        while (true) {
            const Bitboard ourAttackers = attackers & getBb(ourColor);

            if (ourAttackers == 0) break;

            const std::optional<PieceType> optNext = popLeastValuable(ourAttackers);

            if (!optNext) goto attackersUpdated;

            if (*optNext == PieceType::Pawn
            ||  *optNext == PieceType::Bishop
            ||  *optNext == PieceType::Queen)
                attackers |= bishopsQueens & getBishopAttacks(to, occ);

            if (*optNext == PieceType::Rook || *optNext == PieceType::Queen)
                attackers |= rooksQueens & getRookAttacks(to, occ);

            attackersUpdated:

            attackers &= occ;
            ourColor = oppColor(ourColor);

            score = -score - 1;

            if (optNext) score -= SEE_PIECE_VALUES[*optNext];

            // If our only attacker is our king, but the opponent still has defenders
            if (score >= 0
            && optNext
            && *optNext == PieceType::King
            && (attackers & getBb(ourColor)) > 0)
                ourColor = oppColor(ourColor);

            if (score >= 0) break;
        }

        return sideToMove() != ourColor;
    }

    constexpr u64 roughHashAfter(const Move move) const
    {
        u64 hashAfter = zobristHash() ^ ZOBRIST_COLOR;

        if (move == MOVE_NONE) return hashAfter;

        const Square to = move.to();
        const std::optional<PieceType> captured = pieceTypeAt(to);
        const PieceType pt = move.pieceType();

        if (captured)
            hashAfter ^= ZOBRIST_PIECES[notSideToMove()][*captured][to];

        hashAfter ^= ZOBRIST_PIECES[sideToMove()][pt][move.from()];
        hashAfter ^= ZOBRIST_PIECES[sideToMove()][pt][to];

        return hashAfter;
    }

}; // class Position

const Position START_POS = Position(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
);
