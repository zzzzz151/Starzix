#ifndef BOARD_HPP
#define BOARD_HPP

// clang-format off
#include <cstdint>
#include <vector>
#include <random>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "attacks.hpp"
#include "move.hpp"
using namespace std;

struct BoardInfo
{
    public:

    bool castlingRights[2][2]; // castlingRights[color][CASTLE_SHORT or CASTLE_LONG]
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture;
    Piece capturedPiece;

    inline BoardInfo(bool argCastlingRights[2][2], Square argEnPassantTargetSquare, 
                     uint16_t argPliesSincePawnMoveOrCapture, Piece argCapturedPiece)
    {
        castlingRights[WHITE][0] = argCastlingRights[WHITE][0];
        castlingRights[WHITE][1] = argCastlingRights[WHITE][1];
        castlingRights[BLACK][0] = argCastlingRights[BLACK][0];
        castlingRights[BLACK][1] = argCastlingRights[BLACK][1];

        enPassantTargetSquare = argEnPassantTargetSquare;
        pliesSincePawnMoveOrCapture = argPliesSincePawnMoveOrCapture;
        capturedPiece = argCapturedPiece;
    }

};

class Board
{
    public:

    Piece pieces[64];
    uint64_t occupied, piecesBitboards[6][2];
    bool castlingRights[2][2]; // castlingRights[color][isLong]
    const static uint8_t CASTLE_SHORT = 0, CASTLE_LONG = 1;
    Color colorToMove;
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture, currentMoveCounter;
    static inline uint64_t zobristTable[64][12];
    static inline uint64_t zobristTable2[10];
    vector<BoardInfo> states;

    inline Board(string fen)
    {
        states = {};

        vector<string> fenSplit = splitString(fen, ' ');

        parseFenRows(fenSplit[0]);
        colorToMove = fenSplit[1] == "b" ? BLACK : WHITE;
        parseFenCastlingRights(fenSplit[2]);

        string strEnPassantSquare = fenSplit[3];
        enPassantTargetSquare = strEnPassantSquare == "-" ? 0 : strToSquare(strEnPassantSquare);

        pliesSincePawnMoveOrCapture = stoi(fenSplit[4]);
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

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
                placePiece(charToPiece[thisChar], currentRank * 8 + currentFile);
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

    inline string getFen()
    {
        string fen = "";

        for (int rank = 7; rank >= 0; rank--)
        {
            int emptySoFar = 0;
            for (int file = 0; file < 8; file++)
            {
                Square square = rank * 8 + file;
                Piece piece = pieces[square];
                if (piece != Piece::NONE) {
                    if (emptySoFar > 0) fen += to_string(emptySoFar);
                    fen += string(1, pieceToChar[piece]);
                    emptySoFar = 0;
                }
                else
                    emptySoFar++;
            }
            if (emptySoFar > 0) fen += to_string(emptySoFar);
            fen += "/";
        }
        fen.pop_back(); // remove last '/'

        fen += colorToMove == BLACK ? " b " : " w ";

        string strCastlingRights = "";
        if (castlingRights[WHITE][CASTLE_SHORT]) strCastlingRights += "K";
        if (castlingRights[WHITE][CASTLE_LONG]) strCastlingRights += "Q";
        if (castlingRights[BLACK][CASTLE_SHORT]) strCastlingRights += "k";
        if (castlingRights[BLACK][CASTLE_LONG]) strCastlingRights += "q";
        if (strCastlingRights.size() == 0) strCastlingRights = "-";
        fen += strCastlingRights;

        string strEnPassantSquare = enPassantTargetSquare == 0 ? "-" : squareToStr[enPassantTargetSquare];
        fen += " " + strEnPassantSquare;
        
        fen += " " + to_string(pliesSincePawnMoveOrCapture);
        fen += " " + to_string(currentMoveCounter);

        return fen;
    }

    inline void printBoard()
    {
        string str = "";
        for (Square i = 0; i < 8; i++)
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

    inline PieceType pieceTypeAt(Square square)
    {
        return pieceToPieceType(pieces[square]);
    }

    inline PieceType pieceTypeAt(string square)
    {
        return pieceTypeAt(strToSquare(square));
    }

    inline uint64_t getPiecesBitboard(PieceType pieceType, Color color = NULL_COLOR)
    {
        if (color == NULL_COLOR)
            return piecesBitboards[(uint8_t)pieceType][WHITE] | piecesBitboards[(uint8_t)pieceType][BLACK];
        return piecesBitboards[(uint8_t)pieceType][color];
    }

    inline uint64_t getColorBitboard(Color color)
    {
        uint64_t bb = 0;
        for (int pt = 0; pt <= 5; pt++)
            bb |= piecesBitboards[pt][color];
        return bb;
    }

    inline uint64_t getUs()
    {
        return getColorBitboard(colorToMove);
    }

    inline uint64_t getThem()
    {
        return getColorBitboard(colorToMove == WHITE ? BLACK : WHITE);
    }

    inline void placePiece(Piece piece, Square square)
    {
        if (piece == Piece::NONE) return;
        pieces[square] = piece;

        PieceType pieceType = pieceToPieceType(piece);
        Color color = pieceColor(piece);
        uint64_t squareBit = (1ULL << square);

        occupied |= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] |= squareBit;
    }

    inline void removePiece(Square square)
    {
        if (pieces[square] == Piece::NONE) return;
        PieceType pieceType = pieceToPieceType(pieces[square]);
        Color color = pieceColor(pieces[square]);
        pieces[square] = Piece::NONE;
        uint64_t squareBit = (1ULL << square);
        occupied ^= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] ^= squareBit;
    }

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

    inline uint64_t getCastlingHash()
    {
        return castlingRights[WHITE][CASTLE_SHORT] 
               + 2 * castlingRights[WHITE][CASTLE_LONG] 
               + 4 * castlingRights[BLACK][CASTLE_SHORT] 
               + 8 * castlingRights[BLACK][CASTLE_LONG];
    }

    inline uint64_t getZobristHash()
    {
        uint64_t hash = zobristTable2[0];
        if (colorToMove == BLACK)
            hash ^= zobristTable2[1];

        hash ^= getCastlingHash();

        if (enPassantTargetSquare != 0)
            hash ^= zobristTable2[2] / (uint64_t)(squareFile(enPassantTargetSquare) * squareFile(enPassantTargetSquare));

        for (int sq = 0; sq < 64; sq++)
        {
            Piece piece = pieces[sq];
            if (piece == Piece::NONE) continue;
            hash ^= zobristTable[sq][(int)piece];
        }

        return hash;
    }

    inline void makeMove(Move move)
    {
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.getTypeFlag();
        bool capture = isCapture(move);

        pushState(pieces[to]);

        Piece piece = pieces[from];
        removePiece(from);
        removePiece(to);

        if (typeFlag == move.QUEEN_PROMOTION_FLAG)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_QUEEN : Piece::BLACK_QUEEN, to);
            goto piecesProcessed;
        }
        else if (typeFlag == move.KNIGHT_PROMOTION_FLAG)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_KNIGHT : Piece::BLACK_KNIGHT, to);
            goto piecesProcessed;
        }
        else if (typeFlag == move.BISHOP_PROMOTION_FLAG)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_BISHOP : Piece::BLACK_BISHOP, to);
            goto piecesProcessed;
        }
        else if (typeFlag == move.ROOK_PROMOTION_FLAG)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_ROOK : Piece::BLACK_ROOK, to);
            goto piecesProcessed;
        }

        placePiece(piece, to);

        if (typeFlag == move.CASTLING_FLAG)
        {
            if (to == 6) { removePiece(7); placePiece(Piece::WHITE_ROOK, 5); }
            else if (to == 2) { removePiece(0); placePiece(Piece::WHITE_ROOK, 3); }
            else if (to == 62) { removePiece(63); placePiece(Piece::BLACK_ROOK, 61); }
            else if (to == 58) { removePiece(56); placePiece(Piece::BLACK_ROOK, 59); }
        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = colorToMove == WHITE ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
        }

        piecesProcessed:

        PieceType pieceTypeMoved = pieceToPieceType(pieces[to]);
        if (pieceTypeMoved == PieceType::KING || pieceTypeMoved == PieceType::ROOK)
            castlingRights[colorToMove][CASTLE_SHORT] = castlingRights[colorToMove][CASTLE_LONG] = false;

        colorToMove = colorToMove == WHITE ? BLACK : WHITE;
        if (colorToMove == WHITE)
            currentMoveCounter++;

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
            if (!pawnTwoUp) return;
            char file = squareFile(from);

             // we already switched colorToMove
            Square possibleEnPassantTargetSquare = colorToMove == BLACK ? to-8 : to+8;
            Piece enemyPawnPiece = colorToMove == BLACK ? Piece::BLACK_PAWN : Piece::WHITE_PAWN;

            if (file != 'a' && pieces[to-1] == enemyPawnPiece)
                enPassantTargetSquare = possibleEnPassantTargetSquare;
            else if (file != 'h' && pieces[to+1] == enemyPawnPiece)
                enPassantTargetSquare = possibleEnPassantTargetSquare;
        }

    }

    inline void undoMove(Move move)
    {
        colorToMove = colorToMove == WHITE ? BLACK : WHITE;
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.getTypeFlag();

        Piece capturedPiece = states[states.size()-1].capturedPiece;
        Piece piece = pieces[to];
        removePiece(to);

        if (typeFlag == move.CASTLING_FLAG)
        {
            // Replace king
            if (colorToMove == WHITE) 
                placePiece(Piece::WHITE_KING, 4);
            else 
                placePiece(Piece::BLACK_KING, 60);

            // Replace rook
            if (to == 6)
            { 
                removePiece(5); 
                placePiece(Piece::WHITE_ROOK, 7);
            }
            else if (to == 2) 
            {
                removePiece(3);
                placePiece(Piece::WHITE_ROOK, 0);
            }
            else if (to == 62) { 
                removePiece(61);
                placePiece(Piece::BLACK_ROOK, 63);
            }
            else if (to == 58) 
            {
                removePiece(59);
                placePiece(Piece::BLACK_ROOK, 56);
            }
        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from);
            Square capturedPawnSquare = colorToMove == WHITE ? to - 8 : to + 8;
            placePiece(colorToMove == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN, capturedPawnSquare);
        }
        else if (move.getPromotionPieceType() != PieceType::NONE)
        {
            placePiece(colorToMove == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from);
            placePiece(capturedPiece, to); // promotion + capture, so replace the captured piece
        }
        else
        {
            placePiece(piece, from);
            placePiece(capturedPiece, to);
        }

        if (colorToMove == BLACK) 
            currentMoveCounter--;

        pullState();

    }

    inline bool isCapture(Move move)
    {
        if (move.getTypeFlag() == move.EN_PASSANT_FLAG) return true;
        if (pieces[move.to()] != Piece::NONE) return true;
        return false;
    }

    inline void pushState(Piece capturedPiece)
    {
        BoardInfo state = BoardInfo(castlingRights, enPassantTargetSquare, pliesSincePawnMoveOrCapture, capturedPiece);
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

    inline MoveList getMoves(bool capturesOnly = false)
    {
        uint64_t us = getUs();
        uint64_t us2 = us;
        MoveList moveList;

        while (us > 0)
        {
            Square square = poplsb(us);
            uint64_t squareBit = (1ULL << square);
            Piece piece = pieces[square];
            PieceType pieceType = pieceToPieceType(piece);

            if (pieceType == PieceType::PAWN)
                getPawnMoves(square, moveList);
            else if (pieceType == PieceType::KNIGHT)
                getKnightMoves(square, us2, moveList);
            else if (pieceType == PieceType::KING)
                getKingMoves(square, us2, moveList);
        }

        return moveList;
    }

    inline void getPawnMoves(Square square, MoveList &moveList)
    {
        Piece piece = pieces[square];
        Color enemyColor = colorToMove == WHITE ? BLACK : WHITE;
        uint8_t rank = squareRank(square);
        char file = squareFile(square);
        const uint8_t SQUARE_ONE_UP = square + (colorToMove == WHITE ? 8 : -8),
                      SQUARE_TWO_UP = square + (colorToMove == WHITE ? 16 : -16),
                      SQUARE_DIAGONAL_LEFT = square + (colorToMove == WHITE ? 7 : -9),
                      SQUARE_DIAGONAL_RIGHT = square + (colorToMove == WHITE ? 9 : -7);

        // 1 up
        if (pieces[SQUARE_ONE_UP] == Piece::NONE)
            moveList.add(Move(square, SQUARE_ONE_UP, PieceType::PAWN));

        // 2 up
        if ((rank == 1 && colorToMove == WHITE) || (rank == 6 && colorToMove == BLACK))
            if (pieces[SQUARE_TWO_UP] == Piece::NONE)
                moveList.add(Move(square, SQUARE_TWO_UP, PieceType::PAWN));

        // diagonal left
        if (file != 'a')
            if (pieceColor(pieces[SQUARE_DIAGONAL_LEFT]) == enemyColor || enPassantTargetSquare == SQUARE_DIAGONAL_LEFT)
                moveList.add(Move(square, SQUARE_DIAGONAL_LEFT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_DIAGONAL_LEFT));

        // diagonal right
        if (file != 'h')
            if (pieceColor(pieces[SQUARE_DIAGONAL_RIGHT]) == enemyColor || enPassantTargetSquare == SQUARE_DIAGONAL_RIGHT)
                moveList.add(Move(square, SQUARE_DIAGONAL_RIGHT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_DIAGONAL_RIGHT));
    }

    inline void getKnightMoves(Square square, uint64_t us, MoveList &moveList)
    {
        uint64_t thisKnightMoves = knightMoves[square] & ~us;
        while (thisKnightMoves > 0)
        {
            Square targetSquare = poplsb(thisKnightMoves);
            moveList.add(Move(square, targetSquare, PieceType::KNIGHT));
        }
    }

    inline void getKingMoves(Square square, uint64_t us, MoveList &moveList)
    {
        uint64_t thisKingMoves = kingMoves[square] & ~us;
        while (thisKingMoves > 0)
        {
            Square targetSquare = poplsb(thisKingMoves);
            moveList.add(Move(square, targetSquare, PieceType::KNIGHT));
        }
    }


    inline bool isSquareAttacked(int square)
    {
        return false;
    }

    
};

#endif