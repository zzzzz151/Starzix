#pragma once

// clang-format off
#include <array>
#include <vector>
#include <random>
#include <cassert>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "../nnue.hpp"
using namespace std;

struct BoardState
{
    public:

    uint64_t zobristHash;
    array<array<bool, 2>, 2> castlingRights; // color, CASTLE_SHORT/CASTLE_LONG
    Square enPassantSquare;
    uint16_t pliesSincePawnMoveOrCapture;
    Piece capturedPiece;
    Move move;

    inline BoardState(uint64_t zobristHash, array<array<bool, 2>, 2> castlingRights, Square enPassantSquare, 
                      uint16_t pliesSincePawnMoveOrCapture, Piece capturedPiece, Move move)
    {
        this->zobristHash = zobristHash;
        this->castlingRights = castlingRights;
        this->enPassantSquare = enPassantSquare;
        this->pliesSincePawnMoveOrCapture = pliesSincePawnMoveOrCapture;
        this->capturedPiece = capturedPiece;
        this->move = move;
    }

};

class Board
{
    private:

    array<Piece, 64> pieces;
    array<uint64_t, 2> colorBitboard;
    array<array<uint64_t, 6>, 2> bitboards; // color, pieceType

    array<array<bool, 2>, 2> castlingRights; // color, CASTLE_SHORT/CASTLE_LONG
    const static uint8_t CASTLE_SHORT = 0, CASTLE_LONG = 1;

    Color colorToMove;
    Square enPassantSquare; // en passant target square
    uint16_t pliesSincePawnMoveOrCapture, currentMoveCounter;

    vector<BoardState> states;

    uint64_t zobristHash;
    static inline uint64_t zobristTable[64][12],
                           zobristColorToMove,
                           zobristCastlingRights[2][2],
                           zobristEnPassantFiles[8];

    public:

    bool perft = false; // In perft, dont update zobrist hash nor nnue accumulator

    Board() = default;

    inline Board(string fen)
    {
        states.clear();
        states.reserve(256);

        nnue::reset();

        fen = trim(fen);
        vector<string> fenSplit = splitString(fen, ' ');

        colorToMove = fenSplit[1] == "b" ? BLACK : WHITE;
        zobristHash = colorToMove == WHITE ? 0 : zobristColorToMove;

        parseFenRows(fenSplit[0]);
        parseFenCastlingRights(fenSplit[2]);

        string strEnPassantSquare = fenSplit[3];
        enPassantSquare = strEnPassantSquare == "-" ? 255 : strToSquare(strEnPassantSquare);
        if (enPassantSquare != 255)
            zobristHash ^= zobristEnPassantFiles[squareFile(enPassantSquare)];

        pliesSincePawnMoveOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    private:

    inline void parseFenRows(string fenRows)
    {
        for (int sq= 0; sq < 64; sq++)
            pieces[sq] = Piece::NONE;

        colorBitboard[WHITE] = colorBitboard[BLACK] = 0;

        for (int pt = 0; pt < 6; pt++)
            bitboards[WHITE][pt] = bitboards[BLACK][pt] = 0;

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
                Piece piece = charToPiece[thisChar];
                placePiece(sq, piece);
                if (!perft) 
                    updateZobristAndAccumulator(pieceColor(piece), sq, piece, true);
                currentFile++;
            }
        }
    }

    inline void parseFenCastlingRights(string fenCastlingRights)
    {
        castlingRights[WHITE][CASTLE_SHORT] = false;
        castlingRights[WHITE][CASTLE_LONG] = false;
        castlingRights[BLACK][CASTLE_SHORT] = false;
        castlingRights[BLACK][CASTLE_LONG] = false;

        if (fenCastlingRights == "-") 
            return;

        for (int i = 0; i < fenCastlingRights.length(); i++)
        {
            char thisChar = fenCastlingRights[i];
            Color color = isupper(thisChar) ? WHITE : BLACK;
            int castlingRight = thisChar == 'K' || thisChar == 'k' ? CASTLE_SHORT : CASTLE_LONG;
            castlingRights[color][castlingRight] = true;
            zobristHash ^= zobristCastlingRights[color][castlingRight];
        }
    }

    inline void placePiece(Square square, Piece piece)
    {
        if (piece == Piece::NONE) 
            return;

        pieces[square] = piece;
        Color color = pieceColor(piece);

        uint64_t squareBit = (1ULL << square);
        colorBitboard[color] |= squareBit;
        bitboards[color][(uint8_t)pieceToPieceType(piece)] |= squareBit;
    }

    inline void removePiece(Square square)
    {
        if (pieces[square] == Piece::NONE) 
            return;

        Piece piece = pieces[square];
        Color color = pieceColor(piece);
        pieces[square] = Piece::NONE;

        uint64_t squareBit = 1ULL << square;
        colorBitboard[color] ^= squareBit;
        bitboards[color][(uint8_t)pieceToPieceType(piece)] ^= squareBit;
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

    inline string fen()
    {
        string myFen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                Piece piece = pieces[square];
                if (piece != Piece::NONE) {
                    if (emptySoFar > 0) myFen += to_string(emptySoFar);
                    myFen += string(1, pieceToChar[piece]);
                    emptySoFar = 0;
                }
                else
                    emptySoFar++;
            }
            if (emptySoFar > 0) myFen += to_string(emptySoFar);
            myFen += "/";
        }
        myFen.pop_back(); // remove last '/'

        myFen += colorToMove == BLACK ? " b " : " w ";

        string strCastlingRights = "";
        if (castlingRights[WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (castlingRights[WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (castlingRights[BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (castlingRights[BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        string strEnPassantSquare = enPassantSquare == 255 ? "-" : squareToStr[enPassantSquare];
        myFen += " " + strEnPassantSquare;
        
        myFen += " " + to_string(pliesSincePawnMoveOrCapture);
        myFen += " " + to_string(currentMoveCounter);

        return myFen;
    }

    inline void printBoard()
    {
        string str = "";
        for (int i = 7; i >= 0; i--)
        {
            for (Square j = 0; j < 8; j++)
            {
                int square = i * 8 + j;
                str += pieces[square] == Piece::NONE ? "." : string(1, pieceToChar[pieces[square]]);
                str += " ";
            }
            str += "\n";
        }

        cout << str;
    }

    inline auto getPieces() { 
        return pieces;
    }

    inline Color sideToMove() { return colorToMove; }

    inline Color oppSide() { return oppColor(colorToMove); }

    inline Piece pieceAt(Square square) { return pieces[square]; }

    inline Piece pieceAt(string square) { return pieces[strToSquare(square)]; }

    inline PieceType pieceTypeAt(Square square) { return pieceToPieceType(pieces[square]); }

    inline PieceType pieceTypeAt(string square) { return pieceTypeAt(strToSquare(square)); }

    inline uint64_t occupancy() { return colorBitboard[WHITE] | colorBitboard[BLACK]; }

    inline uint64_t getBitboard(PieceType pieceType)
    {   
        return bitboards[WHITE][(int)pieceType] | bitboards[BLACK][(int)pieceType];
    }

    inline uint64_t getBitboard(Color color) { return colorBitboard[color]; }

    inline uint64_t getBitboard(PieceType pieceType, Color color)
    {
        return bitboards[color][(int)pieceType];
    }

    inline uint64_t us() { return colorBitboard[colorToMove]; }

    inline uint64_t them() { return colorBitboard[oppSide()]; }

    inline static void initZobrist()
    {
        random_device rd; // random device to seed the rng
        mt19937_64 gen(rd()); // 64 bit Mersenne Twister rng
        uniform_int_distribution<uint64_t> distribution; // distribution(gen) returns random uint64_t
        
        zobristColorToMove = distribution(gen);

        for (int sq = 0; sq < 64; sq++)
            for (int pt = 0; pt < 12; pt++)
                zobristTable[sq][pt] = distribution(gen);

        zobristCastlingRights[WHITE][CASTLE_SHORT] = distribution(gen);
        zobristCastlingRights[BLACK][CASTLE_SHORT] = distribution(gen);
        zobristCastlingRights[WHITE][CASTLE_LONG] = distribution(gen);
        zobristCastlingRights[BLACK][CASTLE_LONG] = distribution(gen);

        for (int file = 0; file < 8; file++)
            zobristEnPassantFiles[file] = distribution(gen);
    }

    inline uint64_t getZobristHash() { return zobristHash; }

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
        if (pliesSincePawnMoveOrCapture >= 100)
            return true;

        // K vs K
        int numPieces = popcount(occupancy());
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

    private:

    inline void disableCastlingRight(Color color, int castlingRight)
    {
        if (castlingRights[color][castlingRight])
        {
            castlingRights[color][castlingRight] = false;
            zobristHash ^= zobristCastlingRights[color][castlingRight];
        }
    }

    public:

    inline bool makeMove(Move move)
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

        // move piece
        removePiece(from); // remove from source square
        removePiece(to); // remove captured piece if any
        Piece pieceToPlace = move.promotionPieceType() == PieceType::NONE ? pieceMoving : makePiece(move.promotionPieceType(), colorToMove);
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
            capturedSquare = colorToMove == WHITE ? to - 8 : to + 8;
            removePiece(capturedSquare);
        }

        if (inCheck())
        {
            // move is illegal
            undoMove(move, capturedPiece);
            return false;
        }

        // push board state
        BoardState state = BoardState(zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture, capturedPiece, move);
        states.push_back(state); // append

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

        PieceType pieceType = pieceToPieceType(pieceMoving);

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            disableCastlingRight(colorToMove, CASTLE_SHORT);
            disableCastlingRight(colorToMove, CASTLE_LONG);
        }
        if (pieces[0] != Piece::WHITE_ROOK)
            disableCastlingRight(WHITE, CASTLE_LONG);
        if (pieces[7] != Piece::WHITE_ROOK)
            disableCastlingRight(WHITE, CASTLE_SHORT);
        if (pieces[56] != Piece::BLACK_ROOK)
            disableCastlingRight(BLACK, CASTLE_LONG);
        if (pieces[63] != Piece::BLACK_ROOK)
            disableCastlingRight(BLACK, CASTLE_SHORT);

        if (pieceType == PieceType::PAWN || capturedPiece != Piece::NONE)
            pliesSincePawnMoveOrCapture = 0;
        else
            pliesSincePawnMoveOrCapture++;

        if (oppositeColor == WHITE) 
            currentMoveCounter++;

        // if en passant square active, XOR it out of zobrist hash, then reset en passant square
        if (enPassantSquare != 255)
        {
            zobristHash ^= zobristEnPassantFiles[squareFile(enPassantSquare)];
            enPassantSquare = 255;
        }

        // Check if this move created an en passant square
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            uint8_t file = squareFile(from);
            Piece enemyPawn = colorToMove == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN;

            if ((file != 0 && pieces[to-1] == enemyPawn) || (file != 7 && pieces[to+1] == enemyPawn))
            {
                enPassantSquare = colorToMove == WHITE ? to - 8 : to + 8;
                zobristHash ^= zobristEnPassantFiles[file];
            }
        }

        colorToMove = oppositeColor;
        zobristHash ^= zobristColorToMove;

        return true; // move is legal
    }

    inline bool makeMove(string uci)
    {
        return makeMove(Move::fromUci(uci, pieces));
    }

    inline void undoMove(Move illegalMove = NULL_MOVE, Piece illegalyCapturedPiece = Piece::NONE)
    {
        Move move = illegalMove;
        Piece capturedPiece = illegalyCapturedPiece;

        if (illegalMove == NULL_MOVE)
        {
            // undoing a legal move
            move = states.back().move;
            colorToMove = oppSide();
            capturedPiece = states.back().capturedPiece;
            if (colorToMove == BLACK) 
                currentMoveCounter--;
            pullState(); // restore zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture
            nnue::pull(); // pull the previous accumulator
        }

        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.typeFlag();

        Piece pieceMoved = move.promotionPieceType() == PieceType::NONE ? pieces[to] : makePiece(PieceType::PAWN, colorToMove);
        removePiece(to); 
        placePiece(from, pieceMoved);
        if (capturedPiece != Piece::NONE)
        {
            Square capturedSquare = moveFlag == Move::EN_PASSANT_FLAG ? (colorToMove == WHITE ? to-8 : to+8) : to;
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

    inline void pullState()
    {
        assert(states.size() > 0);
        BoardState state = states.back();
        states.pop_back();

        zobristHash = state.zobristHash;
        castlingRights = state.castlingRights;
        enPassantSquare = state.enPassantSquare;
        pliesSincePawnMoveOrCapture = state.pliesSincePawnMoveOrCapture;
    }

    public:

    inline void makeNullMove()
    {
        // push board state
        BoardState state = BoardState(zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture, Piece::NONE, NULL_MOVE);
        states.push_back(state); // append

        colorToMove = oppSide();
        zobristHash ^= zobristColorToMove;

        pliesSincePawnMoveOrCapture++;

        if (colorToMove == WHITE)
            currentMoveCounter++;

        // if en passant square active, XOR it out of zobrist, then reset en passant square
        if (enPassantSquare != 255)
        {
            zobristHash ^= zobristEnPassantFiles[squareFile(enPassantSquare)];
            enPassantSquare = 255;
        }
    }

    inline void undoNullMove()
    {
        pullState(); // restore zobristHash, castlingRights, enPassantSquare, pliesSincePawnMoveOrCapture

        colorToMove = oppSide();
        if (colorToMove == BLACK)
            currentMoveCounter--;
    }

    inline MovesList pseudolegalMoves(bool capturesOnly = false)
    {
        MovesList moves;
        Color enemyColor = oppSide();
        uint64_t us = this->us(),
                 them = this->them(),
                 occupied = us | them,
                 ourPawns = bitboards[colorToMove][(int)PieceType::PAWN],
                 ourKnights = bitboards[colorToMove][(int)PieceType::KNIGHT],
                 ourBishops = bitboards[colorToMove][(int)PieceType::BISHOP],
                 ourRooks = bitboards[colorToMove][(int)PieceType::ROOK],
                 ourQueens = bitboards[colorToMove][(int)PieceType::QUEEN],
                 ourKing = bitboards[colorToMove][(int)PieceType::KING];

        // En passant
        if (enPassantSquare != 255)
        {   
            uint64_t ourEnPassantPawns = attacks::pawnAttacks(enPassantSquare, enemyColor) & ourPawns;
            Piece ourPawn = makePiece(PieceType::PAWN, WHITE);
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
            uint8_t rank = squareRank(sq);
            if (rank == 1)
            {
                pawnHasntMoved = colorToMove == WHITE;
                willPromote = colorToMove == BLACK;
            }
            else if (rank == 6)
            {
                pawnHasntMoved = colorToMove == BLACK;
                willPromote = colorToMove == WHITE;
            }

            uint64_t pawnAttacks = attacks::pawnAttacks(sq, colorToMove) & them;
            while (pawnAttacks > 0)
            {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare);
                else 
                    moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }

            if (capturesOnly)
                continue;

            // pawn pushes (1 square up and 2 squares up)
            Square squareOneUp = colorToMove == WHITE ? sq + 8 : sq - 8;
            if (pieces[squareOneUp] != Piece::NONE)
                continue;
            if (!willPromote)
            {
                // pawn 1 square up
                moves.add(Move(sq, squareOneUp, Move::NORMAL_FLAG));
                // pawn 2 squares up
                Square squareTwoUp = colorToMove == WHITE ? sq + 16 : sq - 16;
                if (pawnHasntMoved && pieces[squareTwoUp] == Piece::NONE)
                    moves.add(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
            }
            else
                addPromotions(moves, sq, squareOneUp);
            
        }

        while (ourKnights > 0)
        {
            Square sq = poplsb(ourKnights);
            uint64_t knightMoves = capturesOnly ? attacks::knightAttacks(sq) & them : attacks::knightAttacks(sq) & ~us;
            while (knightMoves > 0)
            {
                Square targetSquare = poplsb(knightMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        Square kingSquare = poplsb(ourKing);
        uint64_t kingMoves = capturesOnly ? attacks::kingAttacks(kingSquare) & them : attacks::kingAttacks(kingSquare) & ~us;
        while (kingMoves > 0)
        {
            Square targetSquare = poplsb(kingMoves);
            moves.add(Move(kingSquare, targetSquare, Move::NORMAL_FLAG));
        }

        // Castling
        int inCheck = -1;
        if (!capturesOnly)
        {
            if (castlingRights[colorToMove][CASTLE_SHORT]
            && pieces[kingSquare+1] == Piece::NONE
            && pieces[kingSquare+2] == Piece::NONE
            && !(inCheck = this->inCheck())
            && !isSquareAttacked(kingSquare+1, enemyColor) 
            && !isSquareAttacked(kingSquare+2, enemyColor))
                moves.add(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            if (castlingRights[colorToMove][CASTLE_LONG]
            && pieces[kingSquare-1] == Piece::NONE
            && pieces[kingSquare-2] == Piece::NONE
            && pieces[kingSquare-3] == Piece::NONE
            && !(inCheck == -1 ? this->inCheck() : inCheck)
            && !isSquareAttacked(kingSquare-1, enemyColor) 
            && !isSquareAttacked(kingSquare-2, enemyColor))
                moves.add(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }
        
        while (ourBishops > 0)
        {
            Square sq = poplsb(ourBishops);
            uint64_t bishopAttacks = attacks::bishopAttacks(sq, occupied);
            uint64_t bishopMoves = capturesOnly ? bishopAttacks & them : bishopAttacks & ~us;
            while (bishopMoves > 0)
            {
                Square targetSquare = poplsb(bishopMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        while (ourRooks > 0)
        {
            Square sq = poplsb(ourRooks);
            uint64_t rookAttacks = attacks::rookAttacks(sq, occupied);
            uint64_t rookMoves = capturesOnly ? rookAttacks & them : rookAttacks & ~us;
            while (rookMoves > 0)
            {
                Square targetSquare = poplsb(rookMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        while (ourQueens > 0)
        {
            Square sq = poplsb(ourQueens);
            uint64_t queenAttacks = attacks::bishopAttacks(sq, occupied) | attacks::rookAttacks(sq, occupied);
            uint64_t queenMoves = capturesOnly ? queenAttacks & them : queenAttacks & ~us;
            while (queenMoves > 0)
            {
                Square targetSquare = poplsb(queenMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        return moves;
    }

    private:

    inline void addPromotions(MovesList &moves, Square sq, Square targetSquare)
    {
        moves.add(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        moves.add(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
        moves.add(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
        moves.add(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
    }

    public:

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
         //idea: put a super piece in this square and see if its attacks intersect with an enemy piece

        // DEBUG cout << "isSquareAttacked() called on square " << squareToStr[square] << ", colorAttacking " << (int)colorAttacking << endl;

        uint64_t occupied = occupancy();

        // Get the slider pieces of the attacker
        uint64_t attackerBishops = bitboards[colorAttacking][(int)PieceType::BISHOP],
                 attackerRooks = bitboards[colorAttacking][(int)PieceType::ROOK],
                 attackerQueens =  bitboards[colorAttacking][(int)PieceType::QUEEN];

        uint64_t bishopAttacks = attacks::bishopAttacks(square, occupied);
        if ((bishopAttacks & (attackerBishops | attackerQueens)) > 0)
            return true;
 
        // DEBUG cout << "not attacked by bishops" << endl;

        uint64_t rookAttacks = attacks::rookAttacks(square, occupied);
        if ((rookAttacks & (attackerRooks | attackerQueens)) > 0)
            return true;

        // DEBUG cout << "not attacked by rooks" << endl;

        uint64_t knightAttacks = attacks::knightAttacks(square);
        if ((knightAttacks & (bitboards[colorAttacking][(int)PieceType::KNIGHT])) > 0) 
            return true;

         // DEBUG cout << "not attacked by knights" << endl;

        uint64_t kingAttacks = attacks::kingAttacks(square);
        if ((kingAttacks & bitboards[colorAttacking][(int)PieceType::KING]) > 0) 
            return true;

        // DEBUG cout << "not attacked by kings" << endl;

        uint64_t pawnAttacks = attacks::pawnAttacks(square, oppColor(colorAttacking));
        if ((pawnAttacks & bitboards[colorAttacking][(int)PieceType::PAWN]) > 0)
            return true;

        // En passant
        if (enPassantSquare == square && colorAttacking == colorToMove)
            return true;

        // DEBUG cout << "not attacked by pawns" << endl;
        // DEBUG cout << "square not attacked" << endl;

        return false;
    }   

    inline bool isSquareAttacked(string square, Color colorAttacking)
    {
        return isSquareAttacked(strToSquare(square), colorAttacking);
    }

    inline bool inCheck()
    {
        uint64_t ourKingBitboard = bitboards[colorToMove][(int)PieceType::KING];
        Square ourKingSquare = lsb(ourKingBitboard);
        return isSquareAttacked(ourKingSquare, oppSide());
    }

    inline bool hasNonPawnMaterial(Color color = NULL_COLOR)
    {
        // p, n, b, r, q, k
        for (int pt = 1; pt <= 4; pt++)
        {
            if (color == NULL_COLOR)
            {
                if (bitboards[WHITE][pt] > 0 || bitboards[BLACK][pt] > 0)
                    return true;
            }
            else if (bitboards[color][pt] > 0)
                return true;
        }

        return false;
    }

    inline Move getLastMove() 
    { 
        return states.size() > 0 ? states.back().move : NULL_MOVE; 
    }
    
};

