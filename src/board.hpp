#pragma once

// clang-format off
#include <array>
#include <vector>
#include <random>
#include <cassert>
#include "types.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "nnue.hpp"

struct BoardState
{
    public:

    u64 zobristHash;
    std::array<std::array<bool, 2>, 2> castlingRights; // [color][CASTLE_SHORT or CASTLE_LONG]
    Square enPassantSquare;
    u16 pliesSincePawnMoveOrCapture;
    Move move;
    PieceType pieceTypeMoved;
    Piece capturedPiece;
    i8 inCheckCached;

    inline BoardState(u64 zobristHash, std::array<std::array<bool, 2>, 2> castlingRights, 
                      Square enPassantSquare, u16 pliesSincePawnMoveOrCapture, 
                      Move move, PieceType pieceTypeMoved, Piece capturedPiece, 
                      i8 inCheckCached)
    {
        this->zobristHash = zobristHash;
        this->castlingRights = castlingRights;
        this->enPassantSquare = enPassantSquare;
        this->pliesSincePawnMoveOrCapture = pliesSincePawnMoveOrCapture;
        this->move = move;
        this->pieceTypeMoved = pieceTypeMoved;
        this->capturedPiece = capturedPiece;
        this->inCheckCached = inCheckCached;
    }

};

class Board
{
    private:

    std::array<Piece, 64> pieces; // [square]
    
    std::array<u64, 2> colorBitboard;                  // [color]
    std::array<std::array<u64, 6>, 2> piecesBitboards; // [color][pieceType]

    std::array<std::array<bool, 2>, 2> castlingRights; // [color][CASTLE_SHORT or CASTLE_LONG]
    const static u8 CASTLE_SHORT = 0, CASTLE_LONG = 1;

    Color colorToMove;
    Square enPassantSquare; // en passant target square
    u16 pliesSincePawnMoveOrCapture, currentMoveCounter;

    std::vector<BoardState> states;

    u64 zobristHash;
    static inline u64 zobristTable[64][12],
                      zobristColorToMove,
                      zobristCastlingRights[2][2],
                      zobristEnPassantFiles[8];

    i8 inCheckCached = -1; // -1 means invalid (need to calculate inCheck())

    public:

    bool perft = false; // In perft, dont update zobrist hash nor nnue accumulator

    Board() = default;

    inline Board(std::string fen)
    {
        states.clear();
        states.reserve(256);

        perft = false;
        inCheckCached = -1;

        nnue::reset();

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        colorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;
        zobristHash = colorToMove == Color::WHITE ? 0 : zobristColorToMove;

        parseFenRows(fenSplit[0]);
        parseFenCastlingRights(fenSplit[2]);

        std::string strEnPassantSquare = fenSplit[3];
        enPassantSquare = strEnPassantSquare == "-" ? SQUARE_NONE : strToSquare(strEnPassantSquare);
        if (enPassantSquare != SQUARE_NONE)
            zobristHash ^= zobristEnPassantFiles[(int)squareFile(enPassantSquare)];

        pliesSincePawnMoveOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    private:

    inline void parseFenRows(std::string fenRows)
    {
        for (int sq= 0; sq < 64; sq++)
            pieces[sq] = Piece::NONE;

        colorBitboard[(int)Color::WHITE] = colorBitboard[(int)Color::BLACK] = 0;

        for (int pt = 0; pt < 6; pt++)
            piecesBitboards[(int)Color::WHITE][pt] = piecesBitboards[(int)Color::BLACK][pt] = 0;

        int currentRank = 7; // start from top rank
        int currentFile = 0;
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
                Square sq = currentRank * 8 + currentFile;
                Piece piece = CHAR_TO_PIECE[thisChar];
                placePiece(sq, piece);
                if (!perft) 
                    updateZobristAndAccumulator(pieceColor(piece), sq, piece, true);
                currentFile++;
            }
        }
    }

    inline void parseFenCastlingRights(std::string fenCastlingRights)
    {
        castlingRights[(int)Color::WHITE][CASTLE_SHORT] = false;
        castlingRights[(int)Color::WHITE][CASTLE_LONG] = false;
        castlingRights[(int)Color::BLACK][CASTLE_SHORT] = false;
        castlingRights[(int)Color::BLACK][CASTLE_LONG] = false;

        if (fenCastlingRights == "-") 
            return;

        for (int i = 0; i < fenCastlingRights.length(); i++)
        {
            char thisChar = fenCastlingRights[i];
            Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;
            int castlingRight = thisChar == 'K' || thisChar == 'k' ? CASTLE_SHORT : CASTLE_LONG;
            castlingRights[(int)color][castlingRight] = true;
            zobristHash ^= zobristCastlingRights[(int)color][castlingRight];
        }
    }

    inline void placePiece(Square square, Piece piece)
    {
        if (piece == Piece::NONE) 
            return;

        pieces[square] = piece;
        int color = (int)pieceColor(piece);

        u64 squareBit = (1ULL << square);
        colorBitboard[color] |= squareBit;
        piecesBitboards[color][(u8)pieceToPieceType(piece)] |= squareBit;
    }

    inline void removePiece(Square square)
    {
        if (pieces[square] == Piece::NONE) 
            return;

        Piece piece = pieces[square];
        int color = (int)pieceColor(piece);
        pieces[square] = Piece::NONE;

        u64 squareBit = 1ULL << square;
        colorBitboard[color] ^= squareBit;
        piecesBitboards[color][(u8)pieceToPieceType(piece)] ^= squareBit;
    }

    inline void updateZobristAndAccumulator(Color color, Square square, Piece piece, bool activate)
    {
        zobristHash ^= zobristTable[square][(int)piece];
        if (activate)
            nnue::currentAccumulator->activate(color, square, pieceToPieceType(piece));
        else
            nnue::currentAccumulator->deactivate(color, square, pieceToPieceType(piece));
    }

    public:

    inline std::string fen()
    {
        std::string myFen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                Piece piece = pieces[square];
                if (piece != Piece::NONE) {
                    if (emptySoFar > 0) myFen += std::to_string(emptySoFar);
                    myFen += std::string(1, PIECE_TO_CHAR[piece]);
                    emptySoFar = 0;
                }
                else
                    emptySoFar++;
            }
            if (emptySoFar > 0) myFen += std::to_string(emptySoFar);
            myFen += "/";
        }
        myFen.pop_back(); // remove last '/'

        myFen += colorToMove == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (castlingRights[(int)Color::WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (castlingRights[(int)Color::WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (castlingRights[(int)Color::BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (castlingRights[(int)Color::BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        std::string strEnPassantSquare = enPassantSquare == SQUARE_NONE ? "-" : SQUARE_TO_STR[enPassantSquare];
        myFen += " " + strEnPassantSquare;
        
        myFen += " " + std::to_string(pliesSincePawnMoveOrCapture);
        myFen += " " + std::to_string(currentMoveCounter);

        return myFen;
    }

    inline void printBoard()
    {
        std::string str = "";
        for (int i = 7; i >= 0; i--)
        {
            for (Square j = 0; j < 8; j++)
            {
                int square = i * 8 + j;
                str += pieces[square] == Piece::NONE ? "." : std::string(1, PIECE_TO_CHAR[pieces[square]]);
                str += " ";
            }
            str += "\n";
        }

        std::cout << str;
    }

    inline auto getPieces() { 
        return pieces;
    }

    inline Color sideToMove() { return colorToMove; }

    inline Color oppSide() { return oppColor(colorToMove); }

    inline Piece pieceAt(Square square) { return pieces[square]; }

    inline PieceType pieceTypeAt(Square square) { 
        return pieceToPieceType(pieces[square]); 
    }

    inline u64 occupancy() { 
        return colorBitboard[(int)Color::WHITE] | colorBitboard[(int)Color::BLACK]; 
    }

    inline u64 getBitboard(PieceType pieceType) {   
        return piecesBitboards[(int)Color::WHITE][(int)pieceType] 
               | piecesBitboards[(int)Color::BLACK][(int)pieceType];
    }

    inline u64 getBitboard(Color color) { return colorBitboard[(int)color]; }

    inline u64 getBitboard(PieceType pieceType, Color color) {
        return piecesBitboards[(int)color][(int)pieceType];
    }

    inline u64 us() { return colorBitboard[(int)colorToMove]; }

    inline u64 them() { return colorBitboard[(int)oppSide()]; }

    inline static void initZobrist()
    {
        std::mt19937_64 gen(12345); // 64 bit Mersenne Twister rng with seed 12345
        std::uniform_int_distribution<u64> distribution; // distribution(gen) returns random u64
        
        zobristColorToMove = distribution(gen);

        for (int sq = 0; sq < 64; sq++)
            for (int pt = 0; pt < 12; pt++)
                zobristTable[sq][pt] = distribution(gen);

        zobristCastlingRights[(int)Color::WHITE][CASTLE_SHORT] = distribution(gen);
        zobristCastlingRights[(int)Color::BLACK][CASTLE_SHORT] = distribution(gen);
        zobristCastlingRights[(int)Color::WHITE][CASTLE_LONG] = distribution(gen);
        zobristCastlingRights[(int)Color::BLACK][CASTLE_LONG] = distribution(gen);

        for (int file = 0; file < 8; file++)
            zobristEnPassantFiles[file] = distribution(gen);
    }

    inline u64 getZobristHash() { return zobristHash; }

    inline bool isRepetition()
    {
        if (states.size() <= 2) return false;

        for (int i = (int)states.size() - 2; i >= 0 && i >= (int)states.size() - (int)pliesSincePawnMoveOrCapture - 1; i -= 2)
            if (states[i].zobristHash == zobristHash)
                return true;

        return false;
    }

    inline bool isDraw()
    {
        if (isRepetition())
            return true;

        if (pliesSincePawnMoveOrCapture >= 100)
            return true;

        // K vs K
        int numPieces = std::popcount(occupancy());
        if (numPieces == 2)
            return true;

        // KB vs K
        // KN vs K
        if (numPieces == 3 && (getBitboard(PieceType::KNIGHT) > 0 || getBitboard(PieceType::BISHOP) > 0))
            return true;

        return false;
    }

    inline bool isCapture(Move move)
    {
        if (pieceColor(pieces[move.to()]) == oppSide() || move.typeFlag() == Move::EN_PASSANT_FLAG)
            return true;
        return false;
    }

    inline PieceType captured(Move move)
    {
        return move == MOVE_NONE
               ? PieceType::NONE
               : move.typeFlag() == Move::EN_PASSANT_FLAG 
               ? PieceType::PAWN 
               : pieceTypeAt(move.to());
    }

    private:

    inline void disableCastlingRight(Color color, int castlingRight)
    {
        if (castlingRights[(int)color][castlingRight])
        {
            castlingRights[(int)color][castlingRight] = false;
            zobristHash ^= zobristCastlingRights[(int)color][castlingRight];
        }
    }

    public:

    inline bool makeMove(Move move, bool verifyCheckLegality = true)
    {
        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.typeFlag();

        Color oppositeColor = oppSide();
        Piece pieceMoving = pieces[from];
        Piece capturedPiece = pieces[to];
        Square capturedSquare = to;

        Square castlingRookSquareBefore, castlingRookSquareAfter;
        Piece castlingRook;

        removePiece(from); // remove from source square
        removePiece(to); // remove captured piece if any

        Piece pieceToPlace = move.promotion() == PieceType::NONE 
                             ? pieceMoving : makePiece(move.promotion(), colorToMove);
                             
        placePiece(to, pieceToPlace); // place on target square

        if (moveFlag == Move::CASTLING_FLAG)
        {
            // move rook
            castlingRook = makePiece(PieceType::ROOK, colorToMove);
            bool isShortCastle = to > from;
            castlingRookSquareBefore = isShortCastle ? to + 1 : to - 2;
            castlingRookSquareAfter = isShortCastle ? to - 1 : to + 1;
            removePiece(castlingRookSquareBefore); 
            placePiece(castlingRookSquareAfter, makePiece(PieceType::ROOK, colorToMove));
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            // en passant, so remove captured pawn
            capturedPiece = makePiece(PieceType::PAWN, oppositeColor);
            capturedSquare = colorToMove == Color::WHITE ? to - 8 : to + 8;
            removePiece(capturedSquare);
        }

        // Verify that this pseudolegal move is legal
        // (side that played it is not in check after the move)
        if (verifyCheckLegality && inCheckNoCache())
        {
            // move is illegal
            undoMove(move, capturedPiece);
            return false;
        }

        PieceType pieceType = pieceToPieceType(pieceMoving);

        // append board state
        BoardState state = BoardState(zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture,
                                      move, pieceType, capturedPiece, inCheckCached);
        states.push_back(state);

        nnue::push(); // save current accumulator

        if (!perft)
        {
            updateZobristAndAccumulator(colorToMove, from, pieceMoving, false); // remove piece at source square
            updateZobristAndAccumulator(colorToMove, to, pieceToPlace, true); // place piece on target square
            if (capturedPiece != Piece::NONE)
                updateZobristAndAccumulator(oppositeColor, capturedSquare, capturedPiece, false); // remove captured piece
            else if (moveFlag == Move::CASTLING_FLAG)
            {
                updateZobristAndAccumulator(colorToMove, castlingRookSquareBefore, castlingRook, false); // remove castling rook
                updateZobristAndAccumulator(colorToMove, castlingRookSquareAfter, castlingRook, true); // place castling rook
            }
        }

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            disableCastlingRight(colorToMove, CASTLE_SHORT);
            disableCastlingRight(colorToMove, CASTLE_LONG);
        }
        if (pieces[0] != Piece::WHITE_ROOK)
            disableCastlingRight(Color::WHITE, CASTLE_LONG);
        if (pieces[7] != Piece::WHITE_ROOK)
            disableCastlingRight(Color::WHITE, CASTLE_SHORT);
        if (pieces[56] != Piece::BLACK_ROOK)
            disableCastlingRight(Color::BLACK, CASTLE_LONG);
        if (pieces[63] != Piece::BLACK_ROOK)
            disableCastlingRight(Color::BLACK, CASTLE_SHORT);

        if (pieceType == PieceType::PAWN || capturedPiece != Piece::NONE)
            pliesSincePawnMoveOrCapture = 0;
        else
            pliesSincePawnMoveOrCapture++;

        if (oppositeColor == Color::WHITE) 
            currentMoveCounter++;

        // if en passant square active, XOR it out of zobrist hash, then reset en passant square
        if (enPassantSquare != SQUARE_NONE)
        {
            zobristHash ^= zobristEnPassantFiles[(int)squareFile(enPassantSquare)];
            enPassantSquare = SQUARE_NONE;
        }

        // Check if this move created an en passant square
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            File file = squareFile(from);
            Piece enemyPawn = colorToMove == Color::WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN;

            if ((file != File::A && pieces[to-1] == enemyPawn) || (file != File::H && pieces[to+1] == enemyPawn))
            {
                enPassantSquare = colorToMove == Color::WHITE ? to - 8 : to + 8;
                zobristHash ^= zobristEnPassantFiles[(int)file];
            }
        }

        colorToMove = oppositeColor;
        zobristHash ^= zobristColorToMove;

        inCheckCached = -1;

        return true; // move is legal
    }

    inline bool makeMove(std::string uci, bool verifyCheckLegality = true)
    {
        return makeMove(Move::fromUci(uci, pieces), verifyCheckLegality);
    }

    inline void undoMove(Move illegalMove = MOVE_NONE, Piece illegalyCapturedPiece = Piece::NONE)
    {
        Move move = illegalMove;
        Piece capturedPiece = illegalyCapturedPiece;

        if (illegalMove == MOVE_NONE)
        {
            // undoing a legal move
            move = states.back().move;
            colorToMove = oppSide();
            capturedPiece = states.back().capturedPiece;
            if (colorToMove == Color::BLACK) 
                currentMoveCounter--;
            pullState();
            nnue::pull(); // pull the previous accumulator
        }

        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.typeFlag();

        Piece pieceMoved = move.promotion() == PieceType::NONE 
                           ? pieces[to] : makePiece(PieceType::PAWN, colorToMove);

        removePiece(to); 
        placePiece(from, pieceMoved);
        if (capturedPiece != Piece::NONE)
        {
            Square capturedSquare = moveFlag == Move::EN_PASSANT_FLAG 
                                    ? (colorToMove == Color::WHITE ? to-8 : to+8) : to;
            placePiece(capturedSquare, capturedPiece);
        }
        else if (moveFlag == move.CASTLING_FLAG)
        {
            // replace rook
            Piece rook = makePiece(PieceType::ROOK, colorToMove);
            if (to == 6 || to == 62) // king side castle
            {
                removePiece(to - 1); // remove rook
                placePiece(to + 1, rook); // place rook
            }
            else // queen side castle
            {
                removePiece(to + 1); // remove rook
                placePiece(to - 2, rook); // place rook
            }
        }
    }

    private:

    // Restore zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture, inCheckCached
    inline void pullState()
    {
        assert(states.size() > 0);
        BoardState *state = &(states.back());

        zobristHash = state->zobristHash;
        castlingRights = state->castlingRights;
        enPassantSquare = state->enPassantSquare;
        pliesSincePawnMoveOrCapture = state->pliesSincePawnMoveOrCapture;
        inCheckCached = state->inCheckCached;

        states.pop_back();
    }

    public:

    // Do not makeNullMove() in check!
    inline void makeNullMove()
    {
        assert(!inCheckCached);
        
        // append board state
        BoardState state = BoardState(zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture, 
                                      MOVE_NONE, PieceType::NONE, Piece::NONE, inCheckCached);
        states.push_back(state);

        colorToMove = oppSide();
        zobristHash ^= zobristColorToMove;

        pliesSincePawnMoveOrCapture++;

        if (colorToMove == Color::WHITE)
            currentMoveCounter++;

        // if en passant square active, XOR it out of zobrist, then reset en passant square
        if (enPassantSquare != SQUARE_NONE)
        {
            zobristHash ^= zobristEnPassantFiles[(int)squareFile(enPassantSquare)];
            enPassantSquare = SQUARE_NONE;
        }

        inCheckCached = false;
    }

    inline void undoNullMove()
    {
        pullState();

        colorToMove = oppSide();
        if (colorToMove == Color::BLACK)
            currentMoveCounter--;

        assert(!inCheckCached);
    }

    inline MovesList pseudolegalMoves(bool noisyOnly = false, bool underpromotions = true)
    {
        MovesList moves;
        Color enemyColor = oppSide();
        u64 us = this->us(),
                 them = this->them(),
                 occupied = us | them,
                 ourPawns = piecesBitboards[(int)colorToMove][(int)PieceType::PAWN],
                 ourKnights = piecesBitboards[(int)colorToMove][(int)PieceType::KNIGHT],
                 ourBishops = piecesBitboards[(int)colorToMove][(int)PieceType::BISHOP],
                 ourRooks = piecesBitboards[(int)colorToMove][(int)PieceType::ROOK],
                 ourQueens = piecesBitboards[(int)colorToMove][(int)PieceType::QUEEN],
                 ourKing = piecesBitboards[(int)colorToMove][(int)PieceType::KING];

        // En passant
        if (enPassantSquare != SQUARE_NONE)
        {   
            u64 ourEnPassantPawns = attacks::pawnAttacks(enPassantSquare, enemyColor) & ourPawns;
            Piece ourPawn = makePiece(PieceType::PAWN, Color::WHITE);
            while (ourEnPassantPawns > 0)
            {
                Square ourPawnSquare = poplsb(ourEnPassantPawns);
                moves.add(Move(ourPawnSquare, enPassantSquare, Move::EN_PASSANT_FLAG));
            }
        }

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);
            if (rank == Rank::RANK_2)
            {
                pawnHasntMoved = colorToMove == Color::WHITE;
                willPromote = colorToMove == Color::BLACK;
            }
            else if (rank == Rank::RANK_7)
            {
                pawnHasntMoved = colorToMove == Color::BLACK;
                willPromote = colorToMove == Color::WHITE;
            }

            // Generate this pawn's captures
            u64 pawnAttacks = attacks::pawnAttacks(sq, colorToMove) & them;
            while (pawnAttacks > 0)
            {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }

            Square squareOneUp = colorToMove == Color::WHITE ? sq + 8 : sq - 8;
            if (pieces[squareOneUp] != Piece::NONE)
                continue;

            if (willPromote)
            {
                addPromotions(moves, sq, squareOneUp, underpromotions);
                continue;
            }

            if (noisyOnly) continue;

            // pawn 1 square up
            moves.add(Move(sq, squareOneUp, Move::NORMAL_FLAG));
            // pawn 2 squares up
            Square squareTwoUp = colorToMove == Color::WHITE ? sq + 16 : sq - 16;
            if (pawnHasntMoved && pieces[squareTwoUp] == Piece::NONE)
                moves.add(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        while (ourKnights > 0)
        {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = noisyOnly ? attacks::knightAttacks(sq) & them : attacks::knightAttacks(sq) & ~us;
            while (knightMoves > 0)
            {
                Square targetSquare = poplsb(knightMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        Square kingSquare = poplsb(ourKing);
        u64 kingMoves = noisyOnly ? attacks::kingAttacks(kingSquare) & them : attacks::kingAttacks(kingSquare) & ~us;
        while (kingMoves > 0)
        {
            Square targetSquare = poplsb(kingMoves);
            moves.add(Move(kingSquare, targetSquare, Move::NORMAL_FLAG));
        }

        // Castling
        if (!noisyOnly)
        {
            if (castlingRights[(int)colorToMove][CASTLE_SHORT]
            && pieces[kingSquare+1] == Piece::NONE
            && pieces[kingSquare+2] == Piece::NONE
            && !inCheck()
            && !isSquareAttacked(kingSquare+1, enemyColor) 
            && !isSquareAttacked(kingSquare+2, enemyColor))
                moves.add(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            if (castlingRights[(int)colorToMove][CASTLE_LONG]
            && pieces[kingSquare-1] == Piece::NONE
            && pieces[kingSquare-2] == Piece::NONE
            && pieces[kingSquare-3] == Piece::NONE
            && !inCheck()
            && !isSquareAttacked(kingSquare-1, enemyColor) 
            && !isSquareAttacked(kingSquare-2, enemyColor))
                moves.add(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }
        
        while (ourBishops > 0)
        {
            Square sq = poplsb(ourBishops);
            u64 bishopAttacks = attacks::bishopAttacks(sq, occupied);
            u64 bishopMoves = noisyOnly ? bishopAttacks & them : bishopAttacks & ~us;
            while (bishopMoves > 0)
            {
                Square targetSquare = poplsb(bishopMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        while (ourRooks > 0)
        {
            Square sq = poplsb(ourRooks);
            u64 rookAttacks = attacks::rookAttacks(sq, occupied);
            u64 rookMoves = noisyOnly ? rookAttacks & them : rookAttacks & ~us;
            while (rookMoves > 0)
            {
                Square targetSquare = poplsb(rookMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        while (ourQueens > 0)
        {
            Square sq = poplsb(ourQueens);
            u64 queenAttacks = attacks::bishopAttacks(sq, occupied) | attacks::rookAttacks(sq, occupied);
            u64 queenMoves = noisyOnly ? queenAttacks & them : queenAttacks & ~us;
            while (queenMoves > 0)
            {
                Square targetSquare = poplsb(queenMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        return moves;
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

        u64 occupied = occupancy();

        // Get the slider pieces of the attacker
        u64 attackerBishops = piecesBitboards[(int)colorAttacking][(int)PieceType::BISHOP],
                 attackerRooks = piecesBitboards[(int)colorAttacking][(int)PieceType::ROOK],
                 attackerQueens =  piecesBitboards[(int)colorAttacking][(int)PieceType::QUEEN];

        u64 bishopAttacks = attacks::bishopAttacks(square, occupied);
        if ((bishopAttacks & (attackerBishops | attackerQueens)) > 0)
            return true;
 
        u64 rookAttacks = attacks::rookAttacks(square, occupied);
        if ((rookAttacks & (attackerRooks | attackerQueens)) > 0)
            return true;

        u64 knightAttacks = attacks::knightAttacks(square);
        if ((knightAttacks & (piecesBitboards[(int)colorAttacking][(int)PieceType::KNIGHT])) > 0) 
            return true;

        u64 kingAttacks = attacks::kingAttacks(square);
        if ((kingAttacks & piecesBitboards[(int)colorAttacking][(int)PieceType::KING]) > 0) 
            return true;

        u64 pawnAttacks = attacks::pawnAttacks(square, oppColor(colorAttacking));
        if ((pawnAttacks & piecesBitboards[(int)colorAttacking][(int)PieceType::PAWN]) > 0)
            return true;

        // En passant
        if (enPassantSquare == square && colorAttacking == colorToMove)
            return true;

        return false;
    }   

    inline bool inCheck()
    {
        if (inCheckCached != -1) return inCheckCached;

        u64 ourKingBitboard = piecesBitboards[(int)colorToMove][(int)PieceType::KING];
        Square ourKingSquare = lsb(ourKingBitboard);
        return inCheckCached = isSquareAttacked(ourKingSquare, oppSide());
    }

    private:

    // Used internally in makeMove()
    inline bool inCheckNoCache()
    {
        u64 ourKingBitboard = piecesBitboards[(int)colorToMove][(int)PieceType::KING];
        Square ourKingSquare = lsb(ourKingBitboard);
        return isSquareAttacked(ourKingSquare, oppSide());
    }

    public:

    inline bool hasNonPawnMaterial(Color color = Color::NONE)
    {
        if (color == Color::NONE)
            return getBitboard(PieceType::KNIGHT) > 0
                   || getBitboard(PieceType::BISHOP) > 0
                   || getBitboard(PieceType::ROOK) > 0
                   || getBitboard(PieceType::QUEEN) > 0;

        return piecesBitboards[(int)color][(int)PieceType::KNIGHT] > 0
               || piecesBitboards[(int)color][(int)PieceType::BISHOP] > 0
               || piecesBitboards[(int)color][(int)PieceType::ROOK] > 0
               || piecesBitboards[(int)color][(int)PieceType::QUEEN] > 0;
    }

    inline Move getLastMove() 
    { 
        return states.size() > 0 ? states.back().move : MOVE_NONE; 
    }

    inline Move getNthToLastMove(u16 n)
    {
        assert(n >= 1); 
        return states.size() >= n ? states[states.size() - n].move : MOVE_NONE;
    }

    inline PieceType getNthToLastMovePieceType(u16 n)
    {
        assert(n >= 1); 
        return states.size() >= n ? states[states.size() - n].pieceTypeMoved : PieceType::NONE;
    }
    
};

