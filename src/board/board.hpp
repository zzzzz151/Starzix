#ifndef BOARD_HPP
#define BOARD_HPP

// clang-format off
#include <cstdint>
#include <vector>
#include <random>
#include <bitset>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"
#include "../nnue.hpp"
using namespace std;

struct BoardInfo
{
    public:

    uint64_t zobristHash;
    bool castlingRights[2][2]; // castlingRights[color][CASTLE_SHORT or CASTLE_LONG]
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture;
    Piece capturedPiece;
    Move move;

    inline BoardInfo(uint64_t argZobristHash, bool argCastlingRights[2][2], Square argEnPassantTargetSquare, 
                     uint16_t argPliesSincePawnMoveOrCapture, Piece argCapturedPiece, Move argMove)
    {
        castlingRights[WHITE][0] = argCastlingRights[WHITE][0];
        castlingRights[WHITE][1] = argCastlingRights[WHITE][1];
        castlingRights[BLACK][0] = argCastlingRights[BLACK][0];
        castlingRights[BLACK][1] = argCastlingRights[BLACK][1];

        zobristHash = argZobristHash;
        enPassantTargetSquare = argEnPassantTargetSquare;
        pliesSincePawnMoveOrCapture = argPliesSincePawnMoveOrCapture;
        capturedPiece = argCapturedPiece;
        move = argMove;
    }

};

class Board
{
    private:

    Piece pieces[64];
    uint64_t occupied, piecesBitboards[6][2];

    bool castlingRights[2][2]; // color, CASTLE_SHORT/CASTLE_LONG
    const static uint8_t CASTLE_SHORT = 0, CASTLE_LONG = 1;

    Color color;
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture, currentMoveCounter;

    vector<Square> nullMoveEnPassantSquares;    
    vector<BoardInfo> states;

    static inline uint64_t zobristTable[64][12];
    static inline uint64_t zobristTable2[10];
    
    public:

    Board() = default;

    inline Board(string fen)
    {
        nnue::reset();

        states = {};
        nullMoveEnPassantSquares = {};

        fen = trim(fen);
        vector<string> fenSplit = splitString(fen, ' ');

        parseFenRows(fenSplit[0]);
        color = fenSplit[1] == "b" ? BLACK : WHITE;
        parseFenCastlingRights(fenSplit[2]);

        string strEnPassantSquare = fenSplit[3];
        enPassantTargetSquare = strEnPassantSquare == "-" ? 0 : strToSquare(strEnPassantSquare);

        pliesSincePawnMoveOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    private:

    inline void parseFenRows(string fenRows)
    {
        for (int sq= 0; sq < 64; sq++)
            pieces[sq] = Piece::NONE;

        occupied = (uint64_t)0;

        for (int pt = 0; pt < 6; pt++)
            piecesBitboards[pt][WHITE] = piecesBitboards[pt][BLACK] = 0;

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
                placePiece(charToPiece[thisChar], currentRank * 8 + currentFile, true);
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

        if (fenCastlingRights == "-") return;

        for (int i = 0; i < fenCastlingRights.length(); i++)
        {
            char thisChar = fenCastlingRights[i];
            if (thisChar == 'K') castlingRights[WHITE][CASTLE_SHORT] = true;
            else if (thisChar == 'Q') castlingRights[WHITE][CASTLE_LONG] = true;
            else if (thisChar == 'k') castlingRights[BLACK][CASTLE_SHORT] = true;
            else if (thisChar == 'q') castlingRights[BLACK][CASTLE_LONG] = true;
        }
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

        myFen += color == BLACK ? " b " : " w ";

        string strCastlingRights = "";
        if (castlingRights[WHITE][CASTLE_SHORT]) strCastlingRights += "K";
        if (castlingRights[WHITE][CASTLE_LONG]) strCastlingRights += "Q";
        if (castlingRights[BLACK][CASTLE_SHORT]) strCastlingRights += "k";
        if (castlingRights[BLACK][CASTLE_LONG]) strCastlingRights += "q";
        if (strCastlingRights.size() == 0) strCastlingRights = "-";
        myFen += strCastlingRights;

        string strEnPassantSquare = enPassantTargetSquare == 0 ? "-" : squareToStr[enPassantTargetSquare];
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

        // DEBUG cout << str;
    }

    inline void getPieces(Piece* dst) { 
        copy(begin(pieces), end(pieces), dst);
    }

    inline Color colorToMove() { return color; }

    inline Color enemyColor() { return color == WHITE ? BLACK : WHITE; }

    inline Piece pieceAt(Square square) { return pieces[square]; }

    inline Piece pieceAt(string square) { return pieces[strToSquare(square)]; }

    inline PieceType pieceTypeAt(Square square) { return pieceToPieceType(pieces[square]); }

    inline PieceType pieceTypeAt(string square) { return pieceTypeAt(strToSquare(square)); }

    inline uint64_t occupancy() { return occupied; }

    inline uint64_t pieceBitboard(PieceType pieceType, Color argColor = NULL_COLOR)
    {
        if (argColor == NULL_COLOR)
            return piecesBitboards[(uint8_t)pieceType][WHITE] | piecesBitboards[(uint8_t)pieceType][BLACK];
        return piecesBitboards[(uint8_t)pieceType][argColor];
    }

    inline void getPiecesBitboards(uint64_t dst[6][2])
    {
        for (int pt = 0; pt <= 5; pt++)
        {
            dst[pt][WHITE] = piecesBitboards[pt][WHITE];
            dst[pt][BLACK] = piecesBitboards[pt][BLACK];
        }
    }

    inline uint64_t colorBitboard(Color argColor)
    {
        uint64_t bb = 0;
        for (int pt = 0; pt <= 5; pt++)
            bb |= piecesBitboards[pt][argColor];
        return bb;
    }

    inline uint64_t us() { return colorBitboard(color); }

    inline uint64_t them() { return colorBitboard(enemyColor()); }

    private:

    inline void placePiece(Piece piece, Square square, bool updateNnue)
    {
        if (piece == Piece::NONE) return;
        pieces[square] = piece;

        PieceType pieceType = pieceToPieceType(piece);
        Color color = pieceColor(piece);
        uint64_t squareBit = (1ULL << square);

        occupied |= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] |= squareBit;

        if (updateNnue)
            nnue::currentAccumulator->activate(color, pieceType, square);
    }

    inline void removePiece(Square square, bool updateNnue)
    {
        if (pieces[square] == Piece::NONE) return;

        PieceType pieceType = pieceToPieceType(pieces[square]);
        Color color = pieceColor(pieces[square]);

        pieces[square] = Piece::NONE;

        uint64_t squareBit = (1ULL << square);
        occupied ^= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] ^= squareBit;

        if (updateNnue)
            nnue::currentAccumulator->deactivate(color, pieceType, square);
    }

    public:

    inline static void initZobrist()
    {
        random_device rd;  // Create a random device to seed the random number generator
        mt19937_64 gen(rd());  // Create a 64-bit Mersenne Twister random number generator

        for (int sq = 0; sq < 64; sq++)
            for (int pt = 0; pt < 12; pt++)
            {
                uniform_int_distribution<uint64_t> distribution;
                uint64_t randomNum = distribution(gen);
                zobristTable[sq][pt] = randomNum;
            }

        for (int i = 0; i < 10; i++)
        {
            uniform_int_distribution<uint64_t> distribution;
            uint64_t randomNum = distribution(gen);
            zobristTable2[i] = randomNum;
        }
    }

    inline uint64_t zobristHash()
    {
        uint64_t hash = color == WHITE ? zobristTable2[0] : zobristTable2[1];

        uint64_t castlingHash = castlingRights[WHITE][CASTLE_SHORT] 
                                + 2 * castlingRights[WHITE][CASTLE_LONG] 
                                + 4 * castlingRights[BLACK][CASTLE_SHORT] 
                                + 8 * castlingRights[BLACK][CASTLE_LONG];
        hash ^= castlingHash;


        if (enPassantTargetSquare != 0)
            hash ^= zobristTable2[9-squareFile(enPassantTargetSquare)];

        for (int sq = 0; sq < 64; sq++)
        {
            Piece piece = pieces[sq];
            if (piece == Piece::NONE) continue;
            hash ^= zobristTable[sq][(int)piece];
        }

        return hash;
    }

    inline bool isRepetition()
    {
        if (states.size() <= 1) return false;
        uint64_t thisZobristHash = zobristHash();

        for (int i = states.size()-2; i >= 0 && i >= states.size() - pliesSincePawnMoveOrCapture - 1; i -= 2)
            if (states[i].zobristHash == thisZobristHash)
                return true;

        return false;
    }

    inline bool isDraw()
    {
        if (pliesSincePawnMoveOrCapture >= 100)
            return true;

        // K vs K
        int numPieces = popcount(occupied);
        if (numPieces == 2)
            return true;

        // KBK KNK
        if (numPieces == 3 && (pieceBitboard(PieceType::KNIGHT) > 0 || pieceBitboard(PieceType::BISHOP) > 0))
            return true;

        return false;
    }

    inline bool makeMove(Move move)
    {
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.typeFlag();
        bool capture = isCapture(move);
        Color colorPlaying = color;
        Color nextColor = enemyColor();

        nnue::push();
        pushState(move, pieces[to]);

        Piece piece = pieces[from];
        removePiece(from, true);
        removePiece(to, true);

        PieceType promotionPieceType = move.promotionPieceType();
        if (promotionPieceType != PieceType::NONE)
        {
            placePiece(makePiece(promotionPieceType, colorPlaying), to, true);
            goto piecesProcessed;
        }

        placePiece(piece, to, true);

        if (typeFlag == move.CASTLING_FLAG)
        {
            Piece rook = makePiece(PieceType::ROOK, colorPlaying);
            bool isShortCastle = to > from;
            removePiece(isShortCastle ? to+1 : to-2, true); 
            placePiece(rook, isShortCastle ? to-1 : to+1, true);

        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = color == WHITE ? to - 8 : to + 8;
            removePiece(capturedPawnSquare, true);
        }

        piecesProcessed:

        if (nextColor == WHITE) currentMoveCounter++;
        if (inCheck())
        {
            // move is illegal
            color = nextColor;
            undoMove();
            return false;
        }

        PieceType pieceTypeMoved = pieceToPieceType(pieces[to]);
        if (pieceTypeMoved == PieceType::KING) 
            castlingRights[colorPlaying][CASTLE_SHORT] = castlingRights[colorPlaying][CASTLE_LONG] = false;
        else if (piece == Piece::WHITE_ROOK)
        {
            if (from == 0)
                castlingRights[WHITE][CASTLE_LONG] = false;
            else if (from == 7)
                castlingRights[WHITE][CASTLE_SHORT] = false;
        }
        else if (piece == Piece::BLACK_ROOK)
        {
            if (from == 56)
                castlingRights[BLACK][CASTLE_LONG] = false;
            else if (from == 63)
                castlingRights[BLACK][CASTLE_SHORT] = false;
        }

        if (pieceToPieceType(piece) == PieceType::PAWN || capture)
            pliesSincePawnMoveOrCapture = 0;
        else
            pliesSincePawnMoveOrCapture++;

        // Check if this move created an en passant
        enPassantTargetSquare = 0;
        if (pieceToPieceType(piece) == PieceType::PAWN)
        { 
            uint8_t rankFrom = squareRank(from), 
                    rankTo = squareRank(to);

            bool pawnTwoUp = (rankFrom == 1 && rankTo == 3) || (rankFrom == 6 && rankTo == 4);
            if (pawnTwoUp)
            {
                uint8_t file = squareFile(from);

                // we already switched color
                Square possibleEnPassantTargetSquare = colorPlaying == WHITE ? to-8 : to+8;
                Piece enemyPawnPiece =  colorPlaying == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN;

                if (file != 0 && pieces[to-1] == enemyPawnPiece)
                    enPassantTargetSquare = possibleEnPassantTargetSquare;
                else if (file != 7 && pieces[to+1] == enemyPawnPiece)
                    enPassantTargetSquare = possibleEnPassantTargetSquare;
            }
        }

        color = nextColor;
        return true; // move is legal

    }

    inline bool makeMove(string uci)
    {
        Square from = strToSquare(uci.substr(0,2));
        Square to = strToSquare(uci.substr(2,4));

        if (uci.size() == 5) // promotion
        {
            char promotionLowerCase = uci.back(); // last char of string
            uint16_t typeFlag = Move::QUEEN_PROMOTION_FLAG;

            if (promotionLowerCase == 'n') 
                typeFlag = Move::KNIGHT_PROMOTION_FLAG;
            else if (promotionLowerCase == 'b') 
                typeFlag = Move::BISHOP_PROMOTION_FLAG;
            else if (promotionLowerCase == 'r') 
                typeFlag = Move::ROOK_PROMOTION_FLAG;

            return makeMove(Move(from, to, typeFlag));
        }

        PieceType pieceType = pieceToPieceType(pieces[from]);
        if (pieceType == PieceType::PAWN && enPassantTargetSquare != 0 && enPassantTargetSquare == to)
            return makeMove(Move(from, to, Move::EN_PASSANT_FLAG));

        if (pieceType == PieceType::KING)
        {
            int bitboardSquaresMoved = (int)to - (int)from;
            if (bitboardSquaresMoved == 2 || bitboardSquaresMoved == -2)
                return makeMove(Move(from, to, Move::CASTLING_FLAG));
        }

        return makeMove(Move(from, to, Move::NORMAL_FLAG));
    }

    inline void undoMove()
    {
        nnue::pull();
        color = enemyColor();
        Move move = states[states.size()-1].move;
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.typeFlag();

        Piece capturedPiece = states[states.size()-1].capturedPiece;
        Piece piece = pieces[to];
        removePiece(to, false);

        if (typeFlag == move.CASTLING_FLAG)
        {
            // Replace king
            if (color == WHITE) 
                placePiece(Piece::WHITE_KING, 4, false);
            else 
                placePiece(Piece::BLACK_KING, 60, false);

            // Replace rook
            if (to == 6)
            { 
                removePiece(5, false); 
                placePiece(Piece::WHITE_ROOK, 7, false);
            }
            else if (to == 2) 
            {
                removePiece(3, false);
                placePiece(Piece::WHITE_ROOK, 0, false);
            }
            else if (to == 62) { 
                removePiece(61, false);
                placePiece(Piece::BLACK_ROOK, 63, false);
            }
            else if (to == 58) 
            {
                removePiece(59, false);
                placePiece(Piece::BLACK_ROOK, 56, false);
            }
        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            placePiece(color == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from, false);
            Square capturedPawnSquare = color == WHITE ? to - 8 : to + 8;
            placePiece(color == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN, capturedPawnSquare, false);
        }
        else if (move.promotionPieceType() != PieceType::NONE)
        {
            placePiece(color == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from, false);
            placePiece(capturedPiece, to, false); // promotion + capture, so replace the captured piece
        }
        else
        {
            placePiece(piece, from, false);
            placePiece(capturedPiece, to, false);
        }

        if (color == BLACK) 
            currentMoveCounter--;

        pullState();

    }

    inline bool isCapture(Move move)
    {
        if (move.typeFlag() == move.EN_PASSANT_FLAG) 
            return true;
        if (pieceColor(pieces[move.to()]) == enemyColor()) 
            return true;
        return false;
    }

    private:

    inline void pushState(Move move, Piece capturedPiece)
    {
        BoardInfo state = BoardInfo(zobristHash(), castlingRights, enPassantTargetSquare, pliesSincePawnMoveOrCapture, capturedPiece, move);
        states.push_back(state); // append
    }

    inline void pullState()
    {
        BoardInfo state = states[states.size()-1];
        states.pop_back();

        castlingRights[WHITE][CASTLE_SHORT] = state.castlingRights[WHITE][CASTLE_SHORT];
        castlingRights[WHITE][CASTLE_LONG] = state.castlingRights[WHITE][CASTLE_LONG];
        castlingRights[BLACK][CASTLE_SHORT] = state.castlingRights[BLACK][CASTLE_SHORT];
        castlingRights[BLACK][CASTLE_LONG] = state.castlingRights[BLACK][CASTLE_LONG];

        enPassantTargetSquare = state.enPassantTargetSquare;
        pliesSincePawnMoveOrCapture = state.pliesSincePawnMoveOrCapture;
    }

    public:

    inline MovesList pseudolegalMoves(bool capturesOnly = false)
    {
        Color enemy = enemyColor();
        uint64_t usBb = us();
        uint64_t themBb = capturesOnly ? them() : 0;
        MovesList moves;

        uint64_t ourPawns = piecesBitboards[0][color],
                 ourKnights = piecesBitboards[1][color],
                 ourBishops = piecesBitboards[2][color],
                 ourRooks = piecesBitboards[3][color],
                 ourQueens = piecesBitboards[4][color],
                 ourKing = piecesBitboards[5][color];

        while (ourPawns > 0)
        {
            Square sq = poplsb(ourPawns);
            pawnPseudolegalMoves(sq, enemy, moves, capturesOnly);
        }

        while (ourKnights > 0)
        {
            Square sq = poplsb(ourKnights);
            uint64_t knightMoves = capturesOnly ? attacks::knightAttacks(sq) & themBb : attacks::knightAttacks(sq) & ~usBb;
            while (knightMoves > 0)
            {
                Square targetSquare = poplsb(knightMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        Square kingSquare = poplsb(ourKing);
        uint64_t kingMoves = capturesOnly ? attacks::kingAttacks(kingSquare) & themBb : attacks::kingAttacks(kingSquare) & ~usBb;
        while (kingMoves > 0)
        {
            Square targetSquare = poplsb(kingMoves);
            moves.add(Move(kingSquare, targetSquare, Move::NORMAL_FLAG));
        }

        // Castling
        if (!capturesOnly && kingSquare == (color == WHITE ? 4 : 60) && !inCheck())
        {
            if (castlingRights[color][CASTLE_SHORT]
            && pieces[kingSquare+1] == Piece::NONE
            && pieces[kingSquare+2] == Piece::NONE
            && pieces[kingSquare+3] == (color == WHITE ? Piece::WHITE_ROOK : Piece::BLACK_ROOK)
            && !isSquareAttacked(kingSquare+1, enemy) 
            && !isSquareAttacked(kingSquare+2, enemy))
                moves.add(Move(kingSquare, kingSquare+2, Move::CASTLING_FLAG));

            if (castlingRights[color][CASTLE_LONG]
            && pieces[kingSquare-1] == Piece::NONE
            && pieces[kingSquare-2] == Piece::NONE
            && pieces[kingSquare-3] == Piece::NONE
            && pieces[kingSquare-4] == (color == WHITE ? Piece::WHITE_ROOK : Piece::BLACK_ROOK)
            && !isSquareAttacked(kingSquare-1, enemy) 
            && !isSquareAttacked(kingSquare-2, enemy))
                moves.add(Move(kingSquare, kingSquare-2, Move::CASTLING_FLAG));
        }
        
        while (ourBishops > 0)
        {
            Square sq = poplsb(ourBishops);
            uint64_t bishopAttacks = attacks::bishopAttacks(sq, occupied);
            uint64_t bishopMoves = capturesOnly ? bishopAttacks & them()  : bishopAttacks & ~usBb;
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
            uint64_t rookMoves = capturesOnly ? rookAttacks & them()  : rookAttacks & ~usBb;
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
            uint64_t queenMoves = capturesOnly ? queenAttacks & them()  : queenAttacks & ~usBb;
            while (queenMoves > 0)
            {
                Square targetSquare = poplsb(queenMoves);
                moves.add(Move(sq, targetSquare, Move::NORMAL_FLAG));
            }
        }

        return moves;
    }

    private:

    inline void pawnPseudolegalMoves(Square square, Color enemy, MovesList &moves, bool capturesOnly = false)
    {
        Piece piece = pieces[square];
        uint8_t rank = squareRank(square);
        uint8_t file = squareFile(square);
        const int SQUARE_ONE_UP = square + (color == WHITE ? 8 : -8),
                  SQUARE_TWO_UP = square + (color == WHITE ? 16 : -16),
                  SQUARE_DIAGONAL_LEFT = square + (color == WHITE ? 7 : -9),
                  SQUARE_DIAGONAL_RIGHT = square + (color == WHITE ? 9 : -7);

        // diagonal left
        if (file != 0)
        {
            if (enPassantTargetSquare != 0 && enPassantTargetSquare == SQUARE_DIAGONAL_LEFT)
                moves.add(Move(square, SQUARE_DIAGONAL_LEFT, Move::EN_PASSANT_FLAG));
            else if (squareIsBackRank(SQUARE_DIAGONAL_LEFT) && pieceColor(pieces[SQUARE_DIAGONAL_LEFT]) == enemy)
                addPromotions(square, SQUARE_DIAGONAL_LEFT, moves);
            else if (pieceColor(pieces[SQUARE_DIAGONAL_LEFT]) == enemy)
                moves.add(Move(square, SQUARE_DIAGONAL_LEFT, Move::NORMAL_FLAG));
        }

        // diagonal right
        if (file != 7)
        {
            if (enPassantTargetSquare != 0 && enPassantTargetSquare == SQUARE_DIAGONAL_RIGHT)
                moves.add(Move(square, SQUARE_DIAGONAL_RIGHT, Move::EN_PASSANT_FLAG));
            else if (squareIsBackRank(SQUARE_DIAGONAL_RIGHT) && pieceColor(pieces[SQUARE_DIAGONAL_RIGHT]) == enemy)
                addPromotions(square, SQUARE_DIAGONAL_RIGHT, moves);
            else if (pieceColor(pieces[SQUARE_DIAGONAL_RIGHT]) == enemy)
                moves.add(Move(square, SQUARE_DIAGONAL_RIGHT, Move::NORMAL_FLAG));
        }

        if (capturesOnly) return;
        
        // pawn 1 up
        if (pieces[SQUARE_ONE_UP] == Piece::NONE)
        {
            if (squareIsBackRank(SQUARE_ONE_UP))
            {
                addPromotions(square, SQUARE_ONE_UP, moves);
                return;
            }

            moves.add(Move(square, SQUARE_ONE_UP, Move::NORMAL_FLAG));

            // pawn 2 up
            if (pieces[SQUARE_TWO_UP] == Piece::NONE
            && ((rank == 1 && color == WHITE) || (rank == 6 && color == BLACK)))
                moves.add(Move(square, SQUARE_TWO_UP, Move::NORMAL_FLAG));
        }

    }

    inline void addPromotions(Square square, Square targetSquare, MovesList &moves)
    {
        for (uint16_t promotionFlag : Move::PROMOTION_FLAGS)
            moves.add(Move(square, targetSquare, promotionFlag));
    }

    public:

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
         //idea: put a super piece in this square and see if its attacks intersect with an enemy piece

        // printBoard();
        // DEBUG cout << "isSquareAttacked() called on square " << squareToStr[square] << ", colorAttacking " << (int)colorAttacking << endl;

        // Get the slider pieces of the attacker
        uint64_t attackerBishops = piecesBitboards[(int)PieceType::BISHOP][colorAttacking],
                 attackerRooks = piecesBitboards[(int)PieceType::ROOK][colorAttacking],
                 attackerQueens =  piecesBitboards[(int)PieceType::QUEEN][colorAttacking];

        uint64_t bishopAttacks = attacks::bishopAttacks(square, occupied);
        if ((bishopAttacks & (attackerBishops | attackerQueens)) > 0)
            return true;
 
        // DEBUG cout << "not attacked by bishops" << endl;

        uint64_t rookAttacks = attacks::rookAttacks(square, occupied);
        if ((rookAttacks & (attackerRooks | attackerQueens)) > 0)
            return true;

        // DEBUG cout << "not attacked by rooks" << endl;

        uint64_t knightAttacks = attacks::knightAttacks(square);
        if ((knightAttacks & (piecesBitboards[(int)PieceType::KNIGHT][colorAttacking])) > 0) 
            return true;

         // DEBUG cout << "not attacked by knights" << endl;

        uint64_t kingAttacks = attacks::kingAttacks(square);
        if ((kingAttacks & piecesBitboards[(int)PieceType::KING][colorAttacking]) > 0) 
            return true;

        // DEBUG cout << "not attacked by kings" << endl;

        uint64_t pawnAttacks = attacks::pawnAttacks(square, oppColor(colorAttacking));
        if ((pawnAttacks & piecesBitboards[(int)PieceType::PAWN][colorAttacking]) > 0)
            return true;

        // En passant
        if (enPassantTargetSquare != 0 && enPassantTargetSquare == square && colorAttacking == color)
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
        uint64_t ourKingBitboard = piecesBitboards[(int)PieceType::KING][color];
        Square ourKingSquare = lsb(ourKingBitboard);
        return isSquareAttacked(ourKingSquare, enemyColor());
    }

    inline void makeNullMove()
    {
        color = enemyColor();
        if (color == WHITE)
            currentMoveCounter++;

        nullMoveEnPassantSquares.push_back(enPassantTargetSquare); // append
        enPassantTargetSquare = 0;
    }

    inline void undoNullMove()
    {
        color = enemyColor();
        if (color == BLACK)
            currentMoveCounter--;

        enPassantTargetSquare = nullMoveEnPassantSquares[nullMoveEnPassantSquares.size()-1];
        nullMoveEnPassantSquares.pop_back(); // remove last element
    }

    inline bool hasAtLeast1Piece(Color argColor = NULL_COLOR)
    {
        if (argColor == NULL_COLOR) 
            argColor = color;

        // p, n, b, r, q, k
        for (int pt = 1; pt <= 4; pt++)
            if (piecesBitboards[pt][argColor] > 0)
                return true;

        return false;
    }
    
};

#endif