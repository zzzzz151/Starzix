#ifndef BOARD_HPP
#define BOARD_HPP

// clang-format off
#include <cstdint>
#include <vector>
#include "types.hpp"
#include "utils.hpp"
using namespace std;

class Board
{
    public:

    Piece pieces[64];
    uint64_t occupied;
    bool castlingRights[4];
    uint8_t colorToMove;
    uint8_t enPassantTargetSquare;

    Board(string fen)
    {
        // parse the fen into pieces[64]
        // example: rn1qkb1r/ppp1pppp/3p1n2/5b2/3PP3/3B4/PPP2PPP/RNBQK1NR w KQkq - 2 4

        vector<string> fenSplit = splitString(fen, ' ');

        processFenRows(fenSplit[0]);

        colorToMove = fenSplit[1] == "b" ? BLACK : WHITE;

        processCastlingRights(fenSplit[2]);

        string strEnPassantSquare = fenSplit[3];
        enPassantTargetSquare = strEnPassantSquare == "-" ? 0 : stringToSquare(strEnPassantSquare);
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
                    pieces[currentSquare++] = Piece::NONE;
                continue;
            }

            pieces[currentSquare++] = charToPiece[thisChar];
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

        for (uint8_t i = 0; i < 8; i++)
        {
            string fenRow = "";
            int emptySoFar = 0;

            for (uint8_t j = 0; j < 8; j++)
                processSquareToFenRow(fenRow, i * 8 + j, emptySoFar);

            fen = fenRow + "/" + fen;
        }

        return fen;
    }

    inline void processSquareToFenRow(string &fenRow, uint8_t square, int &emptySoFar)
    {
        Piece piece = pieces[square];
        if (squareFile(square) == 'g' && piece != Piece::NONE)
            fenRow += emptySoFar;
        else if (piece != Piece::NONE)
        {
            if (emptySoFar > 0)
            { 
                fenRow += emptySoFar; 
                emptySoFar = 0;
            }
            fenRow += pieceToChar[piece];
        }
        else if (piece == Piece::NONE)
            emptySoFar++;
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

};

#endif