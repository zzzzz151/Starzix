#pragma once

// clang-format off
#include <array>
#include <vector>
#include <random>
#include <cassert>
#include <cstring> // for memset()
#include "types.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "nnue.hpp"

bool zobristInitialiazed = false;
u64 ZOBRIST_COLOR[2],
    ZOBRIST_PIECES[2][6][64],
    ZOBRIST_FILES[8];

inline void initZobrist()
{
    std::mt19937_64 gen(12345); // 64 bit Mersenne Twister rng with seed 12345
    std::uniform_int_distribution<u64> distribution; // distribution(gen) returns random u64

    ZOBRIST_COLOR[0] = distribution(gen);
    ZOBRIST_COLOR[1] = distribution(gen);

    for (int pt = 0; pt < 6; pt++)
        for (int sq = 0; sq < 64; sq++)
        {
            ZOBRIST_PIECES[(int)Color::WHITE][pt][sq] = distribution(gen);
            ZOBRIST_PIECES[(int)Color::BLACK][pt][sq] = distribution(gen);
        }

    for (int file = 0; file < 8; file++)
        ZOBRIST_FILES[file] = distribution(gen);

    zobristInitialiazed = true;
}

struct BoardState
{
    Color colorToMove;
    std::array<u64, 2> colorBitboard;   // [color]
    std::array<u64, 6> piecesBitboards; // [pieceType]
    u64 castlingRights;
    Square enPassantSquare;
    u8 pliesSincePawnOrCapture;
    u16 currentMoveCounter;
    u64 zobristHash = 0;
    i8 inCheckCached = -1; // -1 means invalid (need to calculate inCheck())
    Move lastMove = MOVE_NONE;

    inline BoardState() = default;
};

class Board
{
    BoardState *state = nullptr;
    std::vector<BoardState> states = {};
    Accumulator *accumulator = nullptr;
    std::vector<Accumulator> accumulators = {};
    bool perft = false;

    public:

    inline Board() = default;

    inline Board(std::string fen, bool perft = false)
    {
        if (!zobristInitialiazed) initZobrist();

        this->perft = perft;

        accumulators = {};
        if (!perft)
        {
            accumulators.reserve(256);
            accumulators.push_back(Accumulator());
            accumulator = &accumulators[0];
        }
        else
            accumulator = nullptr;

        states = {};
        states.reserve(256);
        states.push_back(BoardState());
        state = &states[0];

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        state->colorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;
        state->zobristHash = ZOBRIST_COLOR[(int)state->colorToMove];

        // Parse pieces
        memset(state->colorBitboard.data(), 0, sizeof(state->colorBitboard));
        memset(state->piecesBitboards.data(), 0, sizeof(state->piecesBitboards));
        std::string fenRows = fenSplit[0];
        int currentRank = 7, currentFile = 0; // iterate ranks from top to bottom, files from left to right
        for (int i = 0; i < fenRows.length(); i++)
        {
            char thisChar = fenRows[i];
            if (thisChar == '/')
            {
                currentRank--;
                currentFile = 0;
            }
            else if (isdigit(thisChar))
                currentFile += charToInt(thisChar);
            else
            {
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                PieceType pt = CHAR_TO_PIECE_TYPE[thisChar];
                Square sq = currentRank * 8 + currentFile;
                placePiece(color, pt, sq);
                if (!perft)
                    accumulator->activate(color, pt, sq);
                currentFile++;
            }
        }

        // Parse castling rights
        state->castlingRights = 0;
        std::string fenCastlingRights = fenSplit[2];
        if (fenCastlingRights != "-") 
        {
            for (int i = 0; i < fenCastlingRights.length(); i++)
            {
                char thisChar = fenCastlingRights[i];
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
                int castlingRight = thisChar == 'K' || thisChar == 'k' 
                                    ? CASTLE_SHORT : CASTLE_LONG;
                state->castlingRights |= CASTLING_MASKS[(int)color][castlingRight];
            }
            state->zobristHash ^= state->castlingRights;
        }

        // Parse en passant target square
        state->enPassantSquare = SQUARE_NONE;
        std::string strEnPassantSquare = fenSplit[3];
        if (strEnPassantSquare != "-")
        {
            state->enPassantSquare = strToSquare(strEnPassantSquare);
            int file = (int)squareFile(state->enPassantSquare);
            state->zobristHash ^= ZOBRIST_FILES[file];
        }

        // Parse last 2 fen tokens
        state->pliesSincePawnOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        state->currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    inline std::string fen()
    {
        std::string myFen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                Piece piece = pieceAt(square);
                if (piece == Piece::NONE)
                {
                    emptySoFar++;
                    continue;
                }
                if (emptySoFar > 0) 
                    myFen += std::to_string(emptySoFar);
                myFen += std::string(1, PIECE_TO_CHAR[piece]);
                emptySoFar = 0;
            }
            if (emptySoFar > 0) 
                myFen += std::to_string(emptySoFar);
            myFen += "/";
        }

        myFen.pop_back(); // remove last '/'

        myFen += state->colorToMove == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (state->castlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (state->castlingRights & CASTLING_MASKS[(int)Color::WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (state->castlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (state->castlingRights & CASTLING_MASKS[(int)Color::BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        myFen += " ";
        myFen += state->enPassantSquare == SQUARE_NONE 
                 ? "-" : SQUARE_TO_STR[state->enPassantSquare];
        
        myFen += " " + std::to_string(state->pliesSincePawnOrCapture);
        myFen += " " + std::to_string(state->currentMoveCounter);

        return myFen;
    }

    inline void placePiece(Color color, PieceType pieceType, Square square) {
        assert(!isOccupied(square));
        u64 sqBitboard = 1ULL << square;
        state->colorBitboard[(int)color] |= sqBitboard;
        state->piecesBitboards[(int)pieceType] |= sqBitboard;
        state->zobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
    }

    inline void removePiece(Square square) {
        Color color = colorAt(square);
        if (color != Color::NONE)
        {
            u64 sqBitboard = 1ULL << square;
            PieceType pt = pieceTypeAt(square);
            state->colorBitboard[(int)color] ^= sqBitboard;
            state->piecesBitboards[(int)pt] ^= sqBitboard;
            state->zobristHash ^= ZOBRIST_PIECES[(int)color][(int)pt][square];
        }
    }

    inline Color sideToMove() { return state->colorToMove; }

    inline Color oppSide() { 
        return state->colorToMove == Color::WHITE 
               ? Color::BLACK : Color::WHITE; 
    }

    inline PieceType pieceTypeAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < state->piecesBitboards.size(); i++)
            if (sqBitboard & state->piecesBitboards[i])
                return (PieceType)i;
        return PieceType::NONE;
    }

    inline Piece pieceAt(Square square) { 
        u64 sqBitboard = 1ULL << square;
        for (int i = 0; i < state->piecesBitboards.size(); i++)
            if (sqBitboard & state->piecesBitboards[i])
                {
                    Color color = sqBitboard & state->colorBitboard[(int)Color::WHITE]
                                  ? Color::WHITE : Color::BLACK;
                    return makePiece((PieceType)i, color);

                }
        return Piece::NONE;
     }

    inline Color colorAt(Square square) {
        u64 sqBitboard = 1ULL << square;
        if (sqBitboard & state->colorBitboard[(int)Color::WHITE])
            return Color::WHITE;
        if (sqBitboard & state->colorBitboard[(int)Color::BLACK])
            return Color::BLACK;
        return Color::NONE;
    }

    inline u64 occupancy() { 
        return state->colorBitboard[(int)Color::WHITE] 
               | state->colorBitboard[(int)Color::BLACK]; 
    }

    inline bool isOccupied(Square square) {
        return occupancy() & (1ULL << square);
    }

    inline u64 us() { 
        return state->colorBitboard[(int)state->colorToMove]; 
    }

    inline u64 them() { 
        return state->colorBitboard[(int)oppSide()]; 
    }

    inline u64 bitboard(PieceType pieceType) {   
        return state->piecesBitboards[(int)pieceType];
    }

    inline u64 bitboard(Color color) { 
        return state->colorBitboard[(int)color]; 
    }

    inline u64 bitboard(Color color, PieceType pieceType) {
        return state->piecesBitboards[(int)pieceType]
               & state->colorBitboard[(int)color];
    }

    inline void print() { 
        std::string str = "";

        for (int i = 7; i >= 0; i--)
        {
            for (Square j = 0; j < 8; j++)
            {
                int square = i * 8 + j;
                str += pieceAt(square) == Piece::NONE 
                       ? "." 
                       : std::string(1, PIECE_TO_CHAR[pieceAt(square)]);
                str += " ";
            }
            str += "\n";
        }

        std::cout << str;
        std::cout << fen() << std::endl;
        std::cout << "Zobrist hash: " << state->zobristHash << std::endl;
    }

    inline bool isCapture(Move move) {
        assert(move != MOVE_NONE);
        return colorAt(move.to()) == oppSide() 
               || move.flag() == Move::EN_PASSANT_FLAG;
    }

    inline PieceType captured(Move move) {
        assert(move != MOVE_NONE);
        return move.flag() == Move::EN_PASSANT_FLAG
               ? PieceType::PAWN : pieceTypeAt(move.to());
    }

    inline u64 zobristHash() { return state->zobristHash; }

    inline Move lastMove() { return state->lastMove; }

    inline u16 pliesSinceCreation() { return states.size() - 1; }

    inline bool isRepetition()
    {
        if (pliesSinceCreation() < 4) return false;

        for (int i = (int)states.size() - 3; 
        i >= 0 && i >= (int)states.size() - (int)state->pliesSincePawnOrCapture - 2; 
        i -= 2)
            if (states[i].zobristHash == state->zobristHash)
                return true;

        return false;
    }

    inline bool isDraw()
    {
        if (isRepetition() || state->pliesSincePawnOrCapture >= 100)
            return true;

        // K vs K
        int numPieces = std::popcount(occupancy());
        if (numPieces == 2) return true;

        // KB vs K
        // KN vs K
        return numPieces == 3 
        && (bitboard(PieceType::KNIGHT) > 0 
        || bitboard(PieceType::BISHOP) > 0);
    }

    inline bool makeMove(Move move, bool verifyLegality = true)
    {
        states.push_back(*state);
        state = &states.back();

        if (move == MOVE_NONE)
        {
            assert(!state->inCheckCached);

            state->zobristHash ^= ZOBRIST_COLOR[(int)state->colorToMove];
            state->colorToMove = oppSide();
            state->zobristHash ^= ZOBRIST_COLOR[(int)state->colorToMove];

            if (state->enPassantSquare != SQUARE_NONE)
            {
                state->zobristHash ^= ZOBRIST_FILES[(int)squareFile(state->enPassantSquare)];
                state->enPassantSquare = SQUARE_NONE;
            }

            state->pliesSincePawnOrCapture++;

            if (state->colorToMove == Color::WHITE)
                state->currentMoveCounter++;

            state->lastMove = move;
            return true;
        }

        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.flag();
        PieceType promotion = move.promotion();
        PieceType pieceType = move.pieceType();
        Color oppSide = this->oppSide();
        PieceType captured = PieceType::NONE;
        Square capturedPieceSquare = to;

        removePiece(from);

        if (moveFlag == Move::CASTLING_FLAG)
        {
            placePiece(state->colorToMove, PieceType::KING, to);
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            removePiece(rookFrom);
            placePiece(state->colorToMove, PieceType::ROOK, rookTo);
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            capturedPieceSquare = state->colorToMove == Color::WHITE
                                  ? to - 8 : to + 8;
            removePiece(capturedPieceSquare);
            placePiece(state->colorToMove, PieceType::PAWN, to);
            captured = PieceType::PAWN;
        }
        else
        {
            if ((captured = pieceTypeAt(to)) != PieceType::NONE)
                removePiece(to);
            placePiece(state->colorToMove, 
                       promotion != PieceType::NONE ? promotion : pieceType, 
                       to);
        }

        if (verifyLegality && inCheckNoCache())
        {
            states.pop_back();
            state = &states.back();
            return false;
        }

        // Update NNUE accumulator
        if (!perft)
        {
            accumulators.push_back(*accumulator);
            accumulator = &accumulators.back();
            accumulator->deactivate(state->colorToMove, pieceType, from);
            if (captured != PieceType::NONE)
                accumulator->deactivate(oppSide, captured, capturedPieceSquare);
            accumulator->activate(state->colorToMove, promotion != PieceType::NONE ? promotion : pieceType, to);
            if (moveFlag == Move::CASTLING_FLAG)
            {
                auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
                accumulator->deactivate(state->colorToMove, PieceType::ROOK, rookFrom);
                accumulator->activate(state->colorToMove, PieceType::ROOK, rookTo);
            }
        }

        state->zobristHash ^= state->castlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            state->castlingRights &= ~CASTLING_MASKS[(int)state->colorToMove][CASTLE_SHORT]; 
            state->castlingRights &= ~CASTLING_MASKS[(int)state->colorToMove][CASTLE_LONG]; 
        }
        else if ((1ULL << from) & state->castlingRights)
            state->castlingRights &= ~(1ULL << from);
        if ((1ULL << to) & state->castlingRights)
            state->castlingRights &= ~(1ULL << to);

        state->zobristHash ^= state->castlingRights; // XOR new castling rights in

        // Update en passant square
        if (state->enPassantSquare != SQUARE_NONE)
        {
            state->zobristHash ^= ZOBRIST_FILES[(int)squareFile(state->enPassantSquare)];
            state->enPassantSquare = SQUARE_NONE;
        }
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            state->enPassantSquare = state->colorToMove == Color::WHITE
                                     ? to - 8 : to + 8;
            state->zobristHash ^= ZOBRIST_FILES[(int)squareFile(state->enPassantSquare)];
        }

        if (pieceType == PieceType::PAWN || captured != PieceType::NONE)
            state->pliesSincePawnOrCapture = 0;
        else
            state->pliesSincePawnOrCapture++;

        if (state->colorToMove == Color::BLACK)
            state->currentMoveCounter++;

        state->zobristHash ^= ZOBRIST_COLOR[(int)state->colorToMove];
        state->colorToMove = oppSide;
        state->zobristHash ^= ZOBRIST_COLOR[(int)state->colorToMove];

        state->inCheckCached = -1;
        state->lastMove = move;
        return true;
    }

    inline Move uciToMove(std::string uciMove)
    {
        Move move = MOVE_NONE;
        Square from = strToSquare(uciMove.substr(0,2));
        Square to = strToSquare(uciMove.substr(2,4));
        PieceType pieceType = pieceTypeAt(from);
        u16 moveFlag = (u16)pieceType + 1;

        if (uciMove.size() == 5) // promotion
        {
            char promotionLowerCase = uciMove.back(); // last char of string
            if (promotionLowerCase == 'n') 
                moveFlag = Move::KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                moveFlag = Move::BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                moveFlag = Move::ROOK_PROMOTION_FLAG;
            else
                moveFlag = Move::QUEEN_PROMOTION_FLAG;
        }
        else if (pieceType == PieceType::KING)
        {
            if (abs((int)to - (int)from) == 2)
                moveFlag = Move::CASTLING_FLAG;
        }
        else if (pieceType == PieceType::PAWN)
        { 
            int bitboardSquaresTraveled = abs((int)to - (int)from);
            if (bitboardSquaresTraveled == 16)
                moveFlag = Move::PAWN_TWO_UP_FLAG;
            else if (bitboardSquaresTraveled != 8 && !isOccupied(to))
                moveFlag = Move::EN_PASSANT_FLAG;
        }

        return Move(from, to, moveFlag);
    }

    inline bool makeMove(std::string uciMove, bool verifyLegality = true)
    {
        return makeMove(uciToMove(uciMove), verifyLegality);
    }

    inline void undoMove()
    {
        assert(states.size() >= 2);
        if (state->lastMove != MOVE_NONE && !perft)
        {
            assert(accumulators.size() >= 2);
            accumulators.pop_back();
            accumulator = &accumulators.back();
        }
        states.pop_back();
        state = &states.back();
    }

    inline void pseudolegalMoves(MovesList &moves, bool noisyOnly = false, bool underpromotions = true)
    {
        moves.clear();

        Color enemyColor = oppSide();
        u64 us = this->us(),
            them = this->them(),
            occupied = us | them,
            ourPawns = bitboard(state->colorToMove, PieceType::PAWN),
            ourKnights = bitboard(state->colorToMove, PieceType::KNIGHT),
            ourBishops = bitboard(state->colorToMove, PieceType::BISHOP),
            ourRooks = bitboard(state->colorToMove, PieceType::ROOK),
            ourQueens = bitboard(state->colorToMove, PieceType::QUEEN),
            ourKing = bitboard(state->colorToMove, PieceType::KING);

        // En passant
        if (state->enPassantSquare != SQUARE_NONE)
        {   
            u64 ourEnPassantPawns = attacks::pawnAttacks(state->enPassantSquare, enemyColor) & ourPawns;
            while (ourEnPassantPawns > 0)
            {
                Piece ourPawn = makePiece(PieceType::PAWN, state->colorToMove);
                Square ourPawnSquare = poplsb(ourEnPassantPawns);
                moves.add(Move(ourPawnSquare, state->enPassantSquare, Move::EN_PASSANT_FLAG));
            }
        }

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);
            if (rank == Rank::RANK_2)
            {
                pawnHasntMoved = state->colorToMove == Color::WHITE;
                willPromote = state->colorToMove == Color::BLACK;
            }
            else if (rank == Rank::RANK_7)
            {
                pawnHasntMoved = state->colorToMove == Color::BLACK;
                willPromote = state->colorToMove == Color::WHITE;
            }

            // Generate this pawn's captures
            u64 pawnAttacks = attacks::pawnAttacks(sq, state->colorToMove) & them;
            while (pawnAttacks > 0)
            {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.add(Move(sq, targetSquare, Move::PAWN_FLAG));
            }

            Square squareOneUp = state->colorToMove == Color::WHITE ? sq + 8 : sq - 8;
            if (isOccupied(squareOneUp))
                continue;

            if (willPromote)
            {
                addPromotions(moves, sq, squareOneUp, underpromotions);
                continue;
            }

            if (noisyOnly) continue;

            // pawn 1 square up
            moves.add(Move(sq, squareOneUp, Move::PAWN_FLAG));

            // pawn 2 squares up
            Square squareTwoUp = state->colorToMove == Color::WHITE 
                                 ? sq + 16 : sq - 16;
            if (pawnHasntMoved && !isOccupied(squareTwoUp))
                moves.add(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        while (ourKnights > 0)
        {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = attacks::knightAttacks(sq) & (noisyOnly ? them : ~us);
            while (knightMoves > 0)
            {
                Square targetSquare = poplsb(knightMoves);
                moves.add(Move(sq, targetSquare, Move::KNIGHT_FLAG));
            }
        }

        Square kingSquare = poplsb(ourKing);
        u64 kingMoves = attacks::kingAttacks(kingSquare) & (noisyOnly ? them : ~us);
        while (kingMoves > 0)
        {
            Square targetSquare = poplsb(kingMoves);
            moves.add(Move(kingSquare, targetSquare, Move::KING_FLAG));
        }

        // Castling
        if (!noisyOnly)
        {
            if ((state->castlingRights & CASTLING_MASKS[(int)state->colorToMove][CASTLE_SHORT])
            && !isOccupied(kingSquare + 1)
            && !isOccupied(kingSquare + 2)
            && !inCheck()
            && !isSquareAttacked(kingSquare+1, enemyColor) 
            && !isSquareAttacked(kingSquare+2, enemyColor))
                moves.add(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            if ((state->castlingRights & CASTLING_MASKS[(int)state->colorToMove][CASTLE_LONG])
            && !isOccupied(kingSquare - 1)
            && !isOccupied(kingSquare - 2)
            && !isOccupied(kingSquare - 3)
            && !inCheck()
            && !isSquareAttacked(kingSquare-1, enemyColor) 
            && !isSquareAttacked(kingSquare-2, enemyColor))
                moves.add(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }
        
        while (ourBishops > 0)
        {
            Square sq = poplsb(ourBishops);
            u64 bishopMoves = attacks::bishopAttacks(sq, occupied) & (noisyOnly ? them : ~us);
            while (bishopMoves > 0)
            {
                Square targetSquare = poplsb(bishopMoves);
                moves.add(Move(sq, targetSquare, Move::BISHOP_FLAG));
            }
        }

        while (ourRooks > 0)
        {
            Square sq = poplsb(ourRooks);
            u64 rookMoves = attacks::rookAttacks(sq, occupied) & (noisyOnly ? them : ~us);
            while (rookMoves > 0)
            {
                Square targetSquare = poplsb(rookMoves);
                moves.add(Move(sq, targetSquare, Move::ROOK_FLAG));
            }
        }

        while (ourQueens > 0)
        {
            Square sq = poplsb(ourQueens);
            u64 queenMoves = attacks::queenAttacks(sq, occupied) & (noisyOnly ? them : ~us);
            while (queenMoves > 0)
            {
                Square targetSquare = poplsb(queenMoves);
                moves.add(Move(sq, targetSquare, Move::QUEEN_FLAG));
            }
        }
    }

    private:

    inline void addPromotions(MovesList &moves, Square sq, Square targetSquare, bool underpromotions)
    {
        moves.add(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        if (underpromotions)
        {
            moves.add(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
            moves.add(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
            moves.add(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
        }
    }

    public:

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
         // Idea: put a super piece in this square and see if its attacks intersect with an enemy piece

        // Pawn
        if (attacks::pawnAttacks(square, oppColor(colorAttacking)) 
        & bitboard(colorAttacking, PieceType::PAWN))
            return true;

        // Knight
        if (attacks::knightAttacks(square) 
        & bitboard(colorAttacking, PieceType::KNIGHT))
            return true;

        u64 occupied = occupancy();

        // Bishop and queen
        if (attacks::bishopAttacks(square, occupied)
        & (bitboard(colorAttacking, PieceType::BISHOP) 
        | bitboard(colorAttacking, PieceType::QUEEN)))
            return true;
 
        // Rook and queen
        if (attacks::rookAttacks(square, occupied)
        & (bitboard(colorAttacking, PieceType::ROOK) 
        | bitboard(colorAttacking, PieceType::QUEEN)))
            return true;

        // King
        if (attacks::kingAttacks(square) 
        & bitboard(colorAttacking, PieceType::KING)) 
            return true;

        return false;
    }   

    inline bool inCheck()
    {
        if (state->inCheckCached != -1) return state->inCheckCached;

        u64 ourKingBitboard = bitboard(state->colorToMove, PieceType::KING);
        Square ourKingSquare = lsb(ourKingBitboard);
        return state->inCheckCached = isSquareAttacked(ourKingSquare, oppSide());
    }

    private:

    // Used internally in makeMove()
    inline bool inCheckNoCache()
    {
        u64 ourKingBitboard = bitboard(state->colorToMove, PieceType::KING);
        Square ourKingSquare = lsb(ourKingBitboard);
        return isSquareAttacked(ourKingSquare, oppSide());
    }

    public:

    inline bool hasNonPawnMaterial(Color color = Color::NONE)
    {
        if (color == Color::NONE)
            return bitboard(PieceType::KNIGHT) > 0
                   || bitboard(PieceType::BISHOP) > 0
                   || bitboard(PieceType::ROOK) > 0
                   || bitboard(PieceType::QUEEN) > 0;

        return bitboard(color, PieceType::KNIGHT) > 0
               || bitboard(color, PieceType::BISHOP) > 0
               || bitboard(color, PieceType::ROOK) > 0
               || bitboard(color, PieceType::QUEEN) > 0;
    }

    inline Move getNthToLastMove(u16 n)
    {
        assert(n >= 1);
        if ((int)states.size() - (int)n < 0)
            return MOVE_NONE;
        return states[states.size() - n].lastMove;
    }

    inline auto evaluate() {
        return nnue::evaluate(*accumulator, state->colorToMove);
    }

    inline BoardState getBoardState() { return *state; }

    inline Accumulator getAccumulator() { return *accumulator; }

    inline void pushBoardState(BoardState &newState)
    {
        states.push_back(newState);
        state = &states.back();
    }

    inline void pushAccumulator(Accumulator &newAccumulator)
    {
        accumulators.push_back(newAccumulator);
        accumulator = &accumulators.back();
    }
    
};

const Board START_BOARD = Board(START_FEN);

