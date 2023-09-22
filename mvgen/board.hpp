#ifndef BOARD_HPP
#define BOARD_HPP

// clang-format off
#include <cstdint>
#include <vector>
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "move.hpp"
using namespace std;

class Board
{
    public:

    Piece pieces[64];
    uint64_t occupied, piecesBitboards[6][2];
    bool castlingRights[4];
    uint8_t colorToMove;
    uint8_t enPassantTargetSquare;
    uint16_t pliesSincePawnAdvanceOrCapture;
    uint16_t currentMoveCounter;

    inline Board(string fen)
    {
        // parse the fen into pieces[64]
        // example: rn1qkb1r/ppp1pppp/3p1n2/5b2/3PP3/3B4/PPP2PPP/RNBQK1NR w KQkq - 2 4

        occupied = 0;
        for (int i = 0; i < 6; i++)
        {
            piecesBitboards[i][WHITE] = 0;
            piecesBitboards[i][BLACK] = 0;
        }

        vector<string> fenSplit = splitString(fen, ' ');

        processFenRows(fenSplit[0]);

        colorToMove = fenSplit[1] == "b" ? BLACK : WHITE;

        processCastlingRights(fenSplit[2]);

        string strEnPassantSquare = fenSplit[3];
        enPassantTargetSquare = strEnPassantSquare == "-" ? 0 : stringToSquare(strEnPassantSquare);

        pliesSincePawnAdvanceOrCapture = stoi(fenSplit[4]);
        currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;
    }

    inline void processFenRows(string fenRows)
    {
        int currentSquare = 0;
        for (int i = 0; i < fenRows.length(); i++)
        {
            char thisChar = fenRows[i];
            if (thisChar == '/')
                continue;

            if (isdigit(thisChar))
            {
                for (int j = 0; j < charToInt(thisChar); j++)
                    placePiece(Piece::NONE, currentSquare);
                continue;
            }

            placePiece(charToPiece[thisChar], currentSquare);
            currentSquare++;
        }
    }

    inline void processCastlingRights(string fenCastlingRights)
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

        for (int i = 0; i < 8; i++)
        {
            string fenRow = "";
            int emptySoFar = 0;

            for (int j = 0; j < 8; j++)
                processSquareToFenRow(fenRow, i * 8 + j, emptySoFar);
            fen += fenRow + "/";
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
        
        fen += " " + to_string(pliesSincePawnAdvanceOrCapture);
        fen += " " + to_string(currentMoveCounter);

        return fen;
    }

    inline void processSquareToFenRow(string &fenRow, int square, int &emptySoFar)
    {
        Piece piece = pieces[square];
        if (piece == Piece::NONE)
        {
            if (squareFile(square) == 'h')
            {
                fenRow += to_string(emptySoFar + 1);
                emptySoFar = 0;
            }
            else
                emptySoFar++;
        }
        else 
        {
            if (emptySoFar > 0)
            { 
                fenRow += to_string(emptySoFar); 
                emptySoFar = 0;
            }
            fenRow += pieceToChar[piece];
        }

    }

    inline void printBoard()
    {
        string str = "";
        for (uint8_t i = 0; i < 8; i++)
        {
            for (uint8_t j = 0; j < 8; j++)
            {
                int square = i * 8 + j;
                str += pieces[square] == Piece::NONE ? "." : string(1, pieceToChar[pieces[square]]);
                str += " ";
            }
            str += "\n";
        }

        cout << str;
    }

    inline uint64_t getPiecesBitboard(PieceType pieceType, char color = NULL_COLOR)
    {
        if (color == NULL_COLOR)
            return piecesBitboards[(uint8_t)pieceType][WHITE] | piecesBitboards[(uint8_t)pieceType][BLACK];
        return piecesBitboards[(uint8_t)pieceType][color];
    }

    inline void placePiece(Piece piece, uint8_t square)
    {
        PieceType pieceType = pieceToPieceType(piece);
        char color = pieceColor(piece);
        
        pieces[square] = piece;
        uint64_t squareBit = 1ULL << square;
        occupied |= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] |= squareBit;
    }

    inline void removePiece(uint8_t square)
    {
        pieces[square] = Piece::NONE;
    }

    inline void 

};

#endif