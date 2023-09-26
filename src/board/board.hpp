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

    inline static bool initialized = false;

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

    inline static void init()
    {
        initZobrist();
        initMoves();
        initialized = true;
    }

    Board() = default;

    inline Board(string fen)
    {
        if (!initialized)
            init();

        network.ResetAccumulator();
        network.RefreshAccumulator();

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

        cout << str;
    }

    inline Piece* piecesBySquare() { return pieces; }

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

    inline void placePiece(Piece piece, Square square)
    {
        if (piece == Piece::NONE) return;
        pieces[square] = piece;

        PieceType pieceType = pieceToPieceType(piece);
        Color color = pieceColor(piece);
        uint64_t squareBit = (1ULL << square);

        occupied |= squareBit;
        piecesBitboards[(uint8_t)pieceType][color] |= squareBit;

        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>((int)pieceType, (int)color, square);
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

        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>((int)pieceType, (int)color, square);
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

    public:

    inline uint64_t zobristHash()
    {
        uint64_t hash = color == WHITE ? zobristTable2[0] : zobristTable2[1];

        uint64_t castlingHash = castlingRights[WHITE][CASTLE_SHORT] 
                                + 2 * castlingRights[WHITE][CASTLE_LONG] 
                                + 4 * castlingRights[BLACK][CASTLE_SHORT] 
                                + 8 * castlingRights[BLACK][CASTLE_LONG];
        hash ^= castlingHash;

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

    inline bool isRepetition()
    {
        if (states.size() <= 1) return false;
        uint64_t thisZobristHash = zobristHash();

        for (int i = states.size()-2; i >= 0 && i >= states.size() - pliesSincePawnMoveOrCapture - 1; i -= 2)
            if (states[i].zobristHash == thisZobristHash)
                return true;

        return false;
    }

    inline bool makeMove(Move move)
    {
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.typeFlag();
        bool capture = isCapture(move);
        pushState(move, pieces[to]);
        Color colorPlaying = color;
        Color nextColor = enemyColor();

        Piece piece = pieces[from];
        removePiece(from);
        removePiece(to);

        PieceType promotionPieceType = move.promotionPieceType();
        if (promotionPieceType != PieceType::NONE)
        {
            placePiece(makePiece(promotionPieceType, colorPlaying), to);
            goto piecesProcessed;
        }

        placePiece(piece, to);

        if (typeFlag == move.CASTLING_FLAG)
        {
            Piece rook = makePiece(PieceType::ROOK, colorPlaying);
            bool isShortCastle = to > from;
            removePiece(isShortCastle ? to+1 : to-2); 
            placePiece(rook, isShortCastle ? to-1 : to+1);

        }
        else if (typeFlag == move.EN_PASSANT_FLAG)
        {
            Square capturedPawnSquare = color == WHITE ? to - 8 : to + 8;
            removePiece(capturedPawnSquare);
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
                char file = squareFile(from);

                // we already switched color
                Square possibleEnPassantTargetSquare = colorPlaying == WHITE ? to-8 : to+8;
                Piece enemyPawnPiece =  colorPlaying == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN;

                if (file != 'a' && pieces[to-1] == enemyPawnPiece)
                    enPassantTargetSquare = possibleEnPassantTargetSquare;
                else if (file != 'h' && pieces[to+1] == enemyPawnPiece)
                    enPassantTargetSquare = possibleEnPassantTargetSquare;
            }
        }

        color = nextColor;
        return true; // move is legal

    }

    inline bool makeMove(string uci)
    {
        return makeMove(Move::fromUci(uci, piecesBySquare()));
    }

    inline void undoMove()
    {
        color = enemyColor();
        Move move = states[states.size()-1].move;
        Square from = move.from();
        Square to = move.to();
        auto typeFlag = move.typeFlag();

        Piece capturedPiece = states[states.size()-1].capturedPiece;
        Piece piece = pieces[to];
        removePiece(to);

        if (typeFlag == move.CASTLING_FLAG)
        {
            // Replace king
            if (color == WHITE) 
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
            placePiece(color == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from);
            Square capturedPawnSquare = color == WHITE ? to - 8 : to + 8;
            placePiece(color == WHITE ? Piece::BLACK_PAWN : Piece::WHITE_PAWN, capturedPawnSquare);
        }
        else if (move.promotionPieceType() != PieceType::NONE)
        {
            placePiece(color == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN, from);
            placePiece(capturedPiece, to); // promotion + capture, so replace the captured piece
        }
        else
        {
            placePiece(piece, from);
            placePiece(capturedPiece, to);
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
        uint64_t usBb = us();
        uint64_t usBb2 = usBb;
        MovesList moves;

        while (usBb > 0)
        {
            Square square = poplsb(usBb);
            uint64_t squareBit = (1ULL << square);
            Piece piece = pieces[square];
            PieceType pieceType = pieceToPieceType(piece);

            if (pieceType == PieceType::PAWN)
                pawnPseudolegalMoves(square, moves, capturesOnly);
            else if (pieceType == PieceType::KNIGHT)
                knightPseudolegalMoves(square, usBb2, moves, capturesOnly);
            else if (pieceType == PieceType::KING)
                kingPseudolegalMoves(square, usBb2, moves, capturesOnly);
            else
                slidingPiecePseudolegalMoves(square, pieceType, moves, capturesOnly);
        }

        return moves;
    }

    inline void pawnPseudolegalMoves(Square square, MovesList &moves, bool capturesOnly = false)
    {
        Color enemy = enemyColor();
        Piece piece = pieces[square];
        uint8_t rank = squareRank(square);
        char file = squareFile(square);
        const int SQUARE_ONE_UP = square + (color == WHITE ? 8 : -8),
                  SQUARE_TWO_UP = square + (color == WHITE ? 16 : -16),
                  SQUARE_DIAGONAL_LEFT = square + (color == WHITE ? 7 : -9),
                  SQUARE_DIAGONAL_RIGHT = square + (color == WHITE ? 9 : -7);

        // diagonal left
        if (file != 'a')
        {
            if (enPassantTargetSquare != 0 && enPassantTargetSquare == SQUARE_DIAGONAL_LEFT)
                moves.add(Move(square, SQUARE_DIAGONAL_LEFT, Move::EN_PASSANT_FLAG));
            else if (squareIsBackRank(SQUARE_DIAGONAL_LEFT) && pieceColor(pieces[SQUARE_DIAGONAL_LEFT]) == enemy)
                addPromotions(square, SQUARE_DIAGONAL_LEFT, moves);
            else if (pieceColor(pieces[SQUARE_DIAGONAL_LEFT]) == enemy)
                moves.add(Move(square, SQUARE_DIAGONAL_LEFT, Move::NORMAL_FLAG));
        }

        // diagonal right
        if (file != 'h')
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

    private:

    inline void addPromotions(Square square, Square targetSquare, MovesList &moves)
    {
        for (uint16_t promotionFlag : Move::PROMOTION_FLAGS)
            moves.add(Move(square, targetSquare, promotionFlag));
    }

    public:

    inline void knightPseudolegalMoves(Square square, uint64_t usBb, MovesList &moves, bool capturesOnly = false)
    {
        uint64_t thisKnightMoves = knightMoves[square] & ~usBb;
        Color enemy = enemyColor();

        while (thisKnightMoves > 0)
        {
            Square targetSquare = poplsb(thisKnightMoves);
            if (capturesOnly && pieceColor(pieces[targetSquare]) != enemy)
                continue;
            moves.add(Move(square, targetSquare, Move::NORMAL_FLAG));
        }
    }

    inline void kingPseudolegalMoves(Square square, uint64_t usBb, MovesList &moves, bool capturesOnly = false)
    {
        uint64_t thisKingMoves = kingMoves[square] & ~usBb;
        Color enemy = enemyColor();

        while (thisKingMoves > 0)
        {
            Square targetSquare = poplsb(thisKingMoves);
            if (capturesOnly && pieceColor(pieces[targetSquare]) != enemy)
                continue;
            moves.add(Move(square, targetSquare, Move::NORMAL_FLAG));
        }

        if (!capturesOnly && square == (color == WHITE ? 4 : 60) && !inCheck())
        {
            if (castlingRights[color][CASTLE_SHORT]
            && pieces[square+1] == Piece::NONE
            && pieces[square+2] == Piece::NONE
            && pieces[square+3] == (color == WHITE ? Piece::WHITE_ROOK : Piece::BLACK_ROOK)
            && !isSquareAttacked(square+1, enemy) 
            && !isSquareAttacked(square+2, enemy))
                moves.add(Move(square, square+2, Move::CASTLING_FLAG));

            if (castlingRights[color][CASTLE_LONG]
            && pieces[square-1] == Piece::NONE
            && pieces[square-2] == Piece::NONE
            && pieces[square-3] == Piece::NONE
            && pieces[square-4] == (color == WHITE ? Piece::WHITE_ROOK : Piece::BLACK_ROOK)
            && !isSquareAttacked(square-1, enemy) 
            && !isSquareAttacked(square-2, enemy))
                moves.add(Move(square, square-2, Move::CASTLING_FLAG));
        }
    }

    inline void slidingPiecePseudolegalMoves(Square square, PieceType pieceType, MovesList &moves, bool capturesOnly = false)
    {
        int rank = squareRank(square);
        char file = squareFile(square);
        int startIndex = (pieceType == PieceType::BISHOP ? 4 : 0);
        int endIndex = (pieceType == PieceType::ROOK ? 3 : 7);

        for (int dir = startIndex; dir <= endIndex ; dir++)
        {
            int targetRank = rank + DIRECTIONS[dir][RANK_INDEX];
            char targetFile = file + DIRECTIONS[dir][FILE_INDEX];

            while (targetRank >= 0 && targetRank <= 7 && targetFile >= 'a' && targetFile <= 'h')
            {
                int targetSquare = targetRank * 8 + targetFile - 'a';

                Piece pieceHere = pieces[targetSquare];
                if (pieceHere == Piece::NONE)
                { 
                    if (!capturesOnly) 
                        moves.add(Move(square, targetSquare, Move::NORMAL_FLAG));
                }
                else
                {
                    if (pieceColor(pieceHere) != color)
                        // this is a capture
                        moves.add(Move(square, targetSquare, Move::NORMAL_FLAG));
                    break;
                }

                targetRank += DIRECTIONS[dir][RANK_INDEX];
                targetFile +=  DIRECTIONS[dir][FILE_INDEX];
            }
        }
    }

    inline bool isSquareAttacked(Square square, Color colorAttacking)
    {
         //idea: put a super piece in this square and see if its attacks intersect with an enemy piece

        // DEBUG printBoard();
        // DEBUG cout << "isSquareAttacked() called on square " << squareToStr[square] << ", colorAttacking " << (int)colorAttacking << endl;

        // En passant
        if (colorAttacking == color && enPassantTargetSquare != 0 && enPassantTargetSquare == square)
            return true;

        // DEBUG cout << "passed en passant" << endl;

        int rank = squareRank(square);
        char file = squareFile(square);
        Color colorDefending = oppColor(colorAttacking);

        for (int dir = 0; dir < 8; dir++)
        {
            int targetRank = rank + DIRECTIONS[dir][RANK_INDEX];
            char targetFile = file + DIRECTIONS[dir][FILE_INDEX];

            while (targetRank >= 0 && targetRank <= 7 && targetFile >= 'a' && targetFile <= 'h')
            {
                int targetSquare = targetRank * 8 + targetFile - 'a';

                if (pieceColor(pieces[targetSquare]) == colorAttacking)
                {
                    bool isDiagonalDirection = dir > 3;
                    PieceType pieceTypeHere = pieceToPieceType(pieces[targetSquare]);

                    if (pieceTypeHere == PieceType::QUEEN 
                    || (isDiagonalDirection && pieceTypeHere == PieceType::BISHOP)
                    || (!isDiagonalDirection && pieceTypeHere == PieceType::ROOK)) 
                        return true;

                    break;
                }
                else if (pieceColor(pieces[targetSquare]) == colorDefending) // friendly piece here
                    break;

                targetRank += DIRECTIONS[dir][RANK_INDEX];
                targetFile +=  DIRECTIONS[dir][FILE_INDEX];
            }

        }

        // DEBUGcout << "not attacked by sliders" << endl;

        uint64_t thisKnightMoves = knightMoves[square];
        if ((thisKnightMoves & piecesBitboards[(int)PieceType::KNIGHT][colorAttacking]) > 0) return true;

        // DEBUG cout << "not attacked by knights" << endl;

        uint64_t thisKingMoves = kingMoves[square];
        uint64_t attackingKingBb = piecesBitboards[(int)PieceType::KING][colorAttacking];
        if ((thisKingMoves & attackingKingBb) > 0) 
            return true;

        // DEBUG  cout << "not attacked by kings" << endl;
 
        const int SQUARE_DIAGONAL_LEFT = square + (colorAttacking == WHITE ? -9 : 7),
                  SQUARE_DIAGONAL_RIGHT = square + (colorAttacking == WHITE ? -7 : 9);
        Piece pawnAttacking = colorAttacking == WHITE ? Piece::WHITE_PAWN : Piece::BLACK_PAWN;

        if ((colorDefending == WHITE && rank == 7) || (colorDefending == BLACK && rank == 0))
            // in this case, colorDefending cant be attacked by an attacking pawn
            goto afterPawns;

        if ((file != 'a' && pieces[SQUARE_DIAGONAL_LEFT] == pawnAttacking)
        || (file != 'h' && pieces[SQUARE_DIAGONAL_RIGHT] == pawnAttacking))
            return true;

        afterPawns:

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