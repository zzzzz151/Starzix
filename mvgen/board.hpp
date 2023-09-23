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
    public:

    bool castlingRights[4];
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture;
    Piece capturedPiece;

    inline BoardInfo(bool *argCastlingRights, Square argEnPassantTargetSquare, 
    uint16_t argPliesSincePawnMoveOrCapture, Piece argCapturedPiece)
    {
        for (int i = 0; i < 4; i++)
            castlingRights[i] = argCastlingRights[i];

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
    bool castlingRights[4];
    const static uint8_t CASTLING_RIGHT_WHITE_SHORT = 0,
                         CASTLING_RIGHT_WHITE_LONG = 1,
                         CASTLING_RIGHT_BLACK_SHORT = 2,
                         CASTLING_RIGHT_BLACK_LONG = 3;
    Color colorToMove;
    Square enPassantTargetSquare;
    uint16_t pliesSincePawnMoveOrCapture, currentMoveCounter;
    static inline uint64_t zobristTable[64][12];
    static inline uint64_t zobristTable2[10];
    vector<BoardInfo> states;
    uint64_t numStates;

    inline Board(string fen)
    {
        states = {};
        numStates = 0;

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

    inline uint64_t getColorBitboard(Color color)
    {
        uint64_t bb = 0;
        for (int pt = 0; pt <= 5; pt++)
            bb |= piecesBitboards[pt][color];
        return bb;
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

            if (colorToMove == WHITE)
                castlingRights[CASTLING_RIGHT_WHITE_SHORT] = castlingRights[CASTLING_RIGHT_WHITE_LONG] = false;
            else 
                castlingRights[CASTLING_RIGHT_BLACK_SHORT] = castlingRights[CASTLING_RIGHT_BLACK_LONG] = false;
        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = colorToMove == WHITE ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
        }

        piecesProcessed:

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
            if (colorToMove == BLACK)
            {
                if (file == 'a' && pieces[to+1] == Piece::BLACK_PAWN)
                    enPassantTargetSquare = to-8;
                else if (file == 'h' && pieces[to-1] == Piece::BLACK_PAWN)
                    enPassantTargetSquare = to-8;
                else if (pieces[to-1] == Piece::BLACK_PAWN)
                    enPassantTargetSquare = to-8;
                else if (pieces[to+1] == Piece::BLACK_PAWN)
                    enPassantTargetSquare = to-8;
            }
            else
            {
                if (file == 'a' && pieces[to+1] == Piece::WHITE_PAWN)
                    enPassantTargetSquare = to+8;
                else if (file == 'h' && pieces[to-1] == Piece::WHITE_PAWN)
                    enPassantTargetSquare = to+8;
                else if (pieces[to-1] == Piece::WHITE_PAWN)
                    enPassantTargetSquare = to+8;
                else if (pieces[to+1] == Piece::WHITE_PAWN)
                    enPassantTargetSquare = to+8;
            }
        }


    }

    inline void undoMove(Move move)
    {
        colorToMove = colorToMove == WHITE ? BLACK : WHITE;
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.getTypeFlag();

        Piece capturedPiece = states[numStates - 1].capturedPiece;
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
        BoardInfo state = BoardInfo(castlingRights, enPassantTargetSquare, 
                                    pliesSincePawnMoveOrCapture, capturedPiece);

        states.push_back(state); // append
        numStates++;
    }

    inline void pullState()
    {
        BoardInfo state = states[numStates - 1];
        states.pop_back();
        numStates--;

        for (int i = 0; i < 4; i++)
            castlingRights[i] = state.castlingRights[i];

        enPassantTargetSquare = state.enPassantTargetSquare;
        pliesSincePawnMoveOrCapture = state.pliesSincePawnMoveOrCapture;
    }

    inline int getMoves(Move* moves)
    {
        int currentMove = 0;
        uint64_t us = getColorBitboard(colorToMove);

        while (us > 0)
        {
            Square square = poplsb(us);
            uint64_t squareBit = (1ULL << square);
            Piece piece = pieces[square];
            PieceType pieceType = pieceToPieceType(piece);
            vector<Move> thisMoves = {};

            if (pieceType == PieceType::PAWN)
                thisMoves = pawnMoves(square);
            else if (pieceType == PieceType::KNIGHT)
                thisMoves = knightMoves(square);

            for (Move move : thisMoves)
            {
                //cout << "Gennerated move " << move.toUci() << endl;
                moves[currentMove++] = move;
            }
            
        }

        return currentMove; // return num of moves
    }

    inline vector<Move> pawnMoves(Square square)
    {
        Piece piece = pieces[square];
        Color color = pieceColor(piece);
        uint8_t rank = squareRank(square);
        char file = squareFile(square);
        vector<Move> moves = {};

        if (color == WHITE) // white pawn
        {
            const uint8_t SQUARE_ONE_UP = square + 8,
                          SQUARE_TWO_UP  = square + 16,
                          SQUARE_UP_LEFT = square + 7,
                          SQUARE_UP_RIGHT = square + 9;

            // 1 up
            if (pieces[SQUARE_ONE_UP] == Piece::NONE)
                moves.push_back(Move(square, SQUARE_ONE_UP, PieceType::PAWN));

            // 2 up
            if (rank == 1 && pieces[SQUARE_TWO_UP] == Piece::NONE)
                moves.push_back(Move(square, SQUARE_TWO_UP, PieceType::PAWN));

            // up left capture
            if (file != 'a' && (pieceColor(pieces[SQUARE_UP_LEFT]) == BLACK || enPassantTargetSquare == SQUARE_UP_LEFT))
                moves.push_back(Move(square, SQUARE_UP_LEFT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_UP_LEFT));

            // up right capture
            if (file != 'h' && (pieceColor(pieces[SQUARE_UP_RIGHT]) == BLACK || enPassantTargetSquare == SQUARE_UP_RIGHT))
                moves.push_back(Move(square, SQUARE_UP_RIGHT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_UP_RIGHT));
        }
        else // black pawn
        {
            const uint8_t SQUARE_ONE_DOWN = square - 8,
                          SQUARE_TWO_DOWN  = square - 16,
                          SQUARE_DOWN_LEFT = square - 9,
                          SQUARE_DOWN_RIGHT = square - 7;

            // 1 down
            if (pieces[SQUARE_ONE_DOWN] == Piece::NONE)
                moves.push_back(Move(square, SQUARE_ONE_DOWN, PieceType::PAWN));

            // 2 up
            if (rank == 6 && pieces[SQUARE_TWO_DOWN] == Piece::NONE)
                moves.push_back(Move(square, SQUARE_TWO_DOWN, PieceType::PAWN));

            // down right capture
            if (file != 'a' && (pieceColor(pieces[SQUARE_DOWN_LEFT]) == BLACK || enPassantTargetSquare == SQUARE_DOWN_LEFT))
                moves.push_back(Move(square, SQUARE_DOWN_LEFT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_DOWN_LEFT));

            // down left capture
            if (file != 'h' && (pieceColor(pieces[SQUARE_DOWN_RIGHT]) == BLACK || enPassantTargetSquare == SQUARE_DOWN_RIGHT))
                moves.push_back(Move(square, SQUARE_DOWN_RIGHT, PieceType::PAWN, PieceType::NONE, enPassantTargetSquare == SQUARE_DOWN_RIGHT));
        }

        return moves;
    }

    inline vector<Move> knightMoves(Square square)
    {
        vector<Move> moves = {};
        int intSquare = (int)square;
        int possibleTargetSquares[8] = {intSquare-2+8, intSquare-2-8, intSquare+16-1, intSquare+16+1,
                                        intSquare+2+8, intSquare+2-8, intSquare-16-1, intSquare-16+1};

        for (int possibleTargetSquare : possibleTargetSquares)
        {
            if (possibleTargetSquare < 0 || possibleTargetSquare > 63) continue;

            if (abs((int)squareRank(square) - (int)squareRank(possibleTargetSquare)) 
            + abs((int)squareFile(square) - (int)squareFile(possibleTargetSquare))
            != 3)
                continue;

            if (pieceColor(pieces[possibleTargetSquare]) != colorToMove)
                moves.push_back(Move(square, possibleTargetSquare, PieceType::KNIGHT));
        }

        return moves;
    }

    inline bool isSquareAttacked(int square)
    {
        return false;
    }

    
};

#include "move.hpp"

#endif