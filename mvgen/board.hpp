#ifndef BOARD_HPP
#define BOARD_HPP

// clang-format off
#include <cstdint>
#include <vector>
#include <random>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "move.hpp"
using namespace std;

struct BoardInfo
{
    Piece pieces[64];
    uint64_t occupied, piecesBitboards[6][2];
    bool castlingRights[4];
};

class Board
{
    public:

    Piece pieces[64];
    uint64_t occupied, piecesBitboards[6][2];
    bool castlingRights[4];
    const static uint8_t CASTLING_RIGHT_WHITE_SHORT = 0,
                         CASTLING_RIGHT_WHITE_LONG = 1,
                         CASTLING_RIGHT_BLACK_SHORT = 2,
                         CASTLING_RIGHT_BLACK_LONG = 3;
    Color colorToMove;
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture;
    uint16_t currentMoveCounter;
    uint64_t zobristHash;
    static inline uint64_t zobristTable[64][12];
    static inline uint64_t zobristTable2[10];

    inline Board(string fen)
    {
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
        for (int i = 0; i < 4; i++)
            castlingRights[i] = false;

        if (fenCastlingRights == "-") return;

        for (int i = 0; i < fenCastlingRights.length(); i++)
        {
            char thisChar = fenCastlingRights[i];
            if (thisChar == 'K') castlingRights[CASTLING_RIGHT_WHITE_SHORT] = true;
            else if (thisChar == 'Q') castlingRights[CASTLING_RIGHT_WHITE_LONG] = true;
            else if (thisChar == 'k') castlingRights[CASTLING_RIGHT_BLACK_SHORT] = true;
            else if (thisChar == 'q') castlingRights[CASTLING_RIGHT_BLACK_LONG] = true;
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
        if (castlingRights[CASTLING_RIGHT_WHITE_SHORT]) strCastlingRights += "K";
        if (castlingRights[CASTLING_RIGHT_WHITE_LONG]) strCastlingRights += "Q";
        if (castlingRights[CASTLING_RIGHT_BLACK_SHORT]) strCastlingRights += "k";
        if (castlingRights[CASTLING_RIGHT_BLACK_LONG]) strCastlingRights += "q";
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

    inline void placePiece(Piece piece, Square square)
    {
        pieces[square] = piece;

        PieceType pieceType = pieceToPieceType(piece);
        Color color = pieceColor(piece);
        uint64_t squareBit = (1ULL << square);
        if (piece != Piece::NONE)
        {
            occupied |= squareBit;
            piecesBitboards[(uint8_t)pieceType][color] |= squareBit;
        }
        else
        {
            occupied &= ~squareBit;
            piecesBitboards[(uint8_t)pieceType][color] &= ~squareBit;
        }
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
        return castlingRights[0] + 2 * castlingRights[1] + 4 * castlingRights[3] + 8 * castlingRights[4];
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

            if (colorToMove == WHITE)
                castlingRights[CASTLING_RIGHT_WHITE_SHORT] = castlingRights[CASTLING_RIGHT_WHITE_LONG] = false;
            else 
                castlingRights[CASTLING_RIGHT_BLACK_SHORT] = castlingRights[CASTLING_RIGHT_BLACK_LONG] = false;
        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = colorToMove == WHITE ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
            enPassantTargetSquare = 0;
        }

        piecesProcessed:

        colorToMove = colorToMove == WHITE ? BLACK : WHITE;
        if (colorToMove == WHITE)
            currentMoveCounter++;

        if (pieceToPieceType(piece) == PieceType::PAWN || capture)
            pliesSincePawnMoveOrCapture = 0;
        else
            pliesSincePawnMoveOrCapture++;

    }

    inline bool isCapture(Move move)
    {
        if (move.getTypeFlag() == move.EN_PASSANT_FLAG) return true;
        if (pieces[move.to()] != Piece::NONE) return true;
        return false;
    }

    inline vector<Move> getMoves()
    {
        return vector<Move>();
    }

    
};

#include "move.hpp"

#endif