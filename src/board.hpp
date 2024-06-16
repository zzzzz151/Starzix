// clang-format off

#pragma once

#include <random>
#include "types.hpp"
#include "utils.hpp"
#include "move.hpp"
#include "attacks.hpp"

constexpr u8 CASTLE_SHORT = 0, CASTLE_LONG = 1;

u64 ZOBRIST_COLOR = 0;
MultiArray<u64, 2, 6, 64> ZOBRIST_PIECES = {}; // [color][pieceType][square]
std::array<u64, 8> ZOBRIST_FILES = {}; // [file]

#include "cuckoo.hpp"

inline void initZobrist()
{
    std::mt19937_64 gen(12345); // 64 bit Mersenne Twister rng with seed 12345
    std::uniform_int_distribution<u64> distribution; // distribution(gen) returns random u64

    ZOBRIST_COLOR = distribution(gen);

    for (int pt = 0; pt < 6; pt++)
        for (int sq = 0; sq < 64; sq++)
        {
            ZOBRIST_PIECES[WHITE][pt][sq] = distribution(gen);
            ZOBRIST_PIECES[BLACK][pt][sq] = distribution(gen);
        }

    for (int file = 0; file < 8; file++)
        ZOBRIST_FILES[file] = distribution(gen);
}

struct BoardState {
    public:
    Color colorToMove = Color::NONE;
    std::array<u64, 2> colorBitboard = { };   // [color]
    std::array<u64, 6> piecesBitboards = { }; // [pieceType]
    u64 castlingRights = 0;
    Square enPassantSquare = SQUARE_NONE;
    u8 pliesSincePawnOrCapture = 0;
    u16 currentMoveCounter = 1;
    u64 checkers = 0;
    u64 zobristHash = 0;
    u64 pawnHash = 0;
    Move lastMove = MOVE_NONE;
    PieceType captured = PieceType::NONE;
} __attribute__((packed));

class Board {
    private:
    std::vector<BoardState> mStates = {};
    BoardState* mState = nullptr;

    public:

    inline Board() = default;

    // Copy constructor
    Board(const Board& other) {
        mStates = other.mStates;
        mState = mStates.empty() ? nullptr : &mStates.back();
    }

    // Copy assignment operator
    Board& operator=(const Board& other) {
        if (this != &other) {
            mStates = other.mStates;
            mState = mStates.empty() ? nullptr : &mStates.back();
        }
        return *this;
    }

    inline Board(std::string fen)
    {
        mStates = {};
        mStates.reserve(512);
        mStates.push_back(BoardState());
        mState = &mStates[0];

        trim(fen);
        std::vector<std::string> fenSplit = splitString(fen, ' ');

        // Parse color to move
        mState->colorToMove = fenSplit[1] == "b" ? Color::BLACK : Color::WHITE;

        if (mState->colorToMove == Color::BLACK)
            mState->zobristHash ^= ZOBRIST_COLOR;

        // Parse pieces

        mState->colorBitboard = {};
        mState->piecesBitboards = {};

        std::string fenRows = fenSplit[0];
        int currentRank = 7, currentFile = 0; // iterate ranks from top to bottom, files from left to right

        for (u64 i = 0; i < fenRows.length(); i++)
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
                currentFile++;
            }
        }

        // Parse castling rights

        mState->castlingRights = 0;
        std::string fenCastlingRights = fenSplit[2];

        if (fenCastlingRights != "-") 
        {
            for (u64 i = 0; i < fenCastlingRights.length(); i++)
            {
                char thisChar = fenCastlingRights[i];
                Color color = isupper(thisChar) ? Color::WHITE : Color::BLACK;

                int castlingRight = thisChar == 'K' || thisChar == 'k' 
                                    ? CASTLE_SHORT : CASTLE_LONG;

                mState->castlingRights |= CASTLING_MASKS[(int)color][castlingRight];
            }

            mState->zobristHash ^= mState->castlingRights;
        }

        // Parse en passant target square

        mState->enPassantSquare = SQUARE_NONE;
        std::string strEnPassantSquare = fenSplit[3];

        if (strEnPassantSquare != "-")
        {
            mState->enPassantSquare = strToSquare(strEnPassantSquare);
            int file = (int)squareFile(mState->enPassantSquare);
            mState->zobristHash ^= ZOBRIST_FILES[file];
        }

        // Parse last 2 fen tokens
        mState->pliesSincePawnOrCapture = fenSplit.size() >= 5 ? stoi(fenSplit[4]) : 0;
        mState->currentMoveCounter = fenSplit.size() >= 6 ? stoi(fenSplit[5]) : 1;

        Square kingSquare = lsb(getBb(sideToMove(), PieceType::KING));
        mState->checkers = attackers(kingSquare) & them();
    }

    inline Color sideToMove() { return mState->colorToMove; }

    inline Color oppSide() { return oppColor(mState->colorToMove); }

    inline u64 getBb(PieceType pieceType) const {   
        return mState->piecesBitboards[(int)pieceType];
    }

    inline u64 getBb(Color color) const { 
        return mState->colorBitboard[(int)color]; 
    }

    inline u64 getBb(Color color, PieceType pieceType) const {
        return mState->piecesBitboards[(int)pieceType]
               & mState->colorBitboard[(int)color];
    }

    inline u64 us() { 
        return getBb(sideToMove());
    }

    inline u64 them() { 
        return getBb(oppSide());
    }

    inline u64 occupancy() { 
        return getBb(Color::WHITE) | getBb(Color::BLACK);
    }

    inline bool isOccupied(Square square) {
        return occupancy() & (1ULL << square);
    }

    inline u64 checkers() { return mState->checkers; }

    inline bool inCheck() { return mState->checkers > 0; }

    inline u64 zobristHash() { return mState->zobristHash; }

    inline u64 pawnHash() { return mState->pawnHash; }

    inline Move lastMove() { return mState->lastMove; }

    inline Move nthToLastMove(int n) {
        return (int)mStates.size() - n < 0 
               ? MOVE_NONE : mStates[mStates.size() - n].lastMove;
    }

    inline PieceType captured() { return mState->captured; }
    
    inline PieceType pieceTypeAt(Square square) 
    { 
        if (!isOccupied(square)) return PieceType::NONE;

        u64 sqBitboard = 1ULL << square;

        for (int pt = PAWN; pt <= KING; pt++)
            if (sqBitboard & getBb((PieceType)pt))
                return (PieceType)pt;

        return PieceType::NONE;
    }

    inline Piece pieceAt(Square square) { 
        u64 sqBitboard = 1ULL << square;

        for (int pt = PAWN; pt <= KING; pt++)
            if (sqBitboard & getBb((PieceType)pt))
                {
                    Color color = sqBitboard & getBb(Color::WHITE)
                                  ? Color::WHITE : Color::BLACK;

                    return makePiece((PieceType)pt, color);
                }

        return Piece::NONE;
    }

    private:

    inline void placePiece(Color color, PieceType pieceType, Square square) 
    {
        assert(!isOccupied(square));

        mState->colorBitboard[(int)color] |= 1ULL << square;
        mState->piecesBitboards[(int)pieceType] |= 1ULL << square;

        updateHashes(color, pieceType, square);
    }

    inline void removePiece(Color color, PieceType pieceType, Square square) 
    {
        assert(pieceAt(square) == makePiece(pieceType, color));

        mState->colorBitboard[(int)color] ^= 1ULL << square;
        mState->piecesBitboards[(int)pieceType] ^= 1ULL << square;

        updateHashes(color, pieceType, square);
    }

    inline void updateHashes(Color color, PieceType pieceType, Square square)
    {
        mState->zobristHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];

        if (pieceType == PieceType::PAWN)
            mState->pawnHash ^= ZOBRIST_PIECES[(int)color][(int)pieceType][square];
    }

    public:

    inline std::string fen() {
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

        myFen += sideToMove() == Color::BLACK ? " b " : " w ";

        std::string strCastlingRights = "";
        if (mState->castlingRights & CASTLING_MASKS[WHITE][CASTLE_SHORT]) 
            strCastlingRights += "K";
        if (mState->castlingRights & CASTLING_MASKS[WHITE][CASTLE_LONG]) 
            strCastlingRights += "Q";
        if (mState->castlingRights & CASTLING_MASKS[BLACK][CASTLE_SHORT]) 
            strCastlingRights += "k";
        if (mState->castlingRights & CASTLING_MASKS[BLACK][CASTLE_LONG]) 
            strCastlingRights += "q";

        if (strCastlingRights.size() == 0) 
            strCastlingRights = "-";

        myFen += strCastlingRights;

        myFen += " ";
        myFen += mState->enPassantSquare == SQUARE_NONE 
                 ? "-" : SQUARE_TO_STR[mState->enPassantSquare];
        
        myFen += " " + std::to_string(mState->pliesSincePawnOrCapture);
        myFen += " " + std::to_string(mState->currentMoveCounter);

        return myFen;
    }

    inline void print() { 
        std::string str = "";

        for (int i = 7; i >= 0; i--) {
            for (int j = 0; j < 8; j++) 
            {
                int square = i * 8 + j;

                str += pieceAt(square) == Piece::NONE 
                       ? "." 
                       : std::string(1, PIECE_TO_CHAR[pieceAt(square)]);

                str += " ";
            }
            str += "\n";
        }

        std::cout << str << std::endl;
        std::cout << fen() << std::endl;
        std::cout << "Zobrist hash: " << mState->zobristHash << std::endl;
        std::cout << "Checkers: " << mState->checkers << std::endl;
        std::cout << "Pinned: " << pinned() << std::endl;

        if (lastMove() != MOVE_NONE)
            std::cout << "Last move: " << lastMove().toUci() << std::endl;
    }

    inline bool isCapture(Move move) {
        assert(move != MOVE_NONE);
        return isOccupied(move.to()) || move.flag() == Move::EN_PASSANT_FLAG;
    }

    inline PieceType captured(Move move) {
        assert(move != MOVE_NONE);

        return move.flag() == Move::EN_PASSANT_FLAG
               ? PieceType::PAWN 
               : pieceTypeAt(move.to());
    }

    inline bool isRepetition(int searchPly = 100000) {
        assert(searchPly >= 0);
        
        if (mStates.size() <= 4 || mState->pliesSincePawnOrCapture < 4) 
            return false;

        int stateIdxAfterPawnOrCapture = std::max(0, (int)mStates.size() - (int)mState->pliesSincePawnOrCapture - 1);
        int rootStateIdx = (int)mStates.size() - searchPly - 1;
        int count = 0;

        for (int i = (int)mStates.size() - 3; i >= stateIdxAfterPawnOrCapture; i -= 2)
            if (mStates[i].zobristHash == mState->zobristHash
            && (i > rootStateIdx || ++count == 2))
                return true;

        return false;
    }

    inline bool isDraw(int searchPly) {
        if (mState->pliesSincePawnOrCapture >= 100)
            return true;

        // K vs K
        int numPieces = std::popcount(occupancy());
        if (numPieces == 2) return true;

        // KB vs K
        // KN vs K
        if (numPieces == 3 
        && (getBb(PieceType::KNIGHT) > 0 || getBb(PieceType::BISHOP) > 0))
            return true;

        return isRepetition(searchPly);
    }

    inline bool isSquareAttacked(Square square, Color colorAttacking, u64 occ = ONES) {
         // Idea: put a super piece in this square and see if its attacks intersect with an enemy piece

        if (occ == ONES) occ = occupancy();

        // Pawn
        if (attacks::pawnAttacks(square, oppColor(colorAttacking)) 
        & getBb(colorAttacking, PieceType::PAWN))
            return true;

        // Knight
        if (attacks::knightAttacks(square) 
        & getBb(colorAttacking, PieceType::KNIGHT))
            return true;

        // Bishop and queen
        if (attacks::bishopAttacks(square, occ)
        & (getBb(colorAttacking, PieceType::BISHOP) 
        | getBb(colorAttacking, PieceType::QUEEN)))
            return true;
 
        // Rook and queen
        if (attacks::rookAttacks(square, occ)
        & (getBb(colorAttacking, PieceType::ROOK) 
        | getBb(colorAttacking, PieceType::QUEEN)))
            return true;

        // King
        if (attacks::kingAttacks(square) 
        & getBb(colorAttacking, PieceType::KING)) 
            return true;

        return false;
    }

    inline u64 attackers(Square square, u64 occ = ONES) {
        if (occ == ONES) occ = occupancy();

        u64 bishopsQueens = getBb(PieceType::BISHOP) | getBb(PieceType::QUEEN);
        u64 rooksQueens   = getBb(PieceType::ROOK)   | getBb(PieceType::QUEEN);

        u64 attackers = getBb(Color::BLACK, PieceType::PAWN) 
                        & attacks::pawnAttacks(square, Color::WHITE);

        attackers |= getBb(Color::WHITE, PieceType::PAWN) 
                     & attacks::pawnAttacks(square, Color::BLACK);

        attackers |= getBb(PieceType::KNIGHT) & attacks::knightAttacks(square);

        attackers |= bishopsQueens & attacks::bishopAttacks(square, occ);
        attackers |= rooksQueens   & attacks::rookAttacks(square, occ);

        attackers |= getBb(PieceType::KING) & attacks::kingAttacks(square);

        return attackers;
    }

    inline u64 attacks(Color color)
    {
        u64 attacksBb = 0;

        u64 pawns = getBb(color) & getBb(PieceType::PAWN);
        while (pawns) {
            Square sq = poplsb(pawns);
            attacksBb |= attacks::pawnAttacks(sq, color);
        }

        u64 knights = getBb(color) & getBb(PieceType::KNIGHT);
        while (knights) {
            Square sq = poplsb(knights);
            attacksBb |= attacks::knightAttacks(sq);
        }

        u64 bishopsQueens = getBb(color) & (getBb(PieceType::BISHOP) | getBb(PieceType::QUEEN));

        while (bishopsQueens) {
            Square sq = poplsb(bishopsQueens);
            attacksBb |= attacks::bishopAttacks(sq, occupancy());
        }

        u64 rooksQueens = getBb(color) & (getBb(PieceType::ROOK) | getBb(PieceType::QUEEN));

        while (rooksQueens) {
            Square sq = poplsb(rooksQueens);
            attacksBb |= attacks::rookAttacks(sq, occupancy());
        }

        Square kingSquare = lsb(getBb(color, PieceType::KING));
        attacksBb |= attacks::kingAttacks(kingSquare);

        return attacksBb;
    } 

    inline u64 pinned() { 
        Square kingSquare = lsb(getBb(sideToMove(), PieceType::KING));
        u64 theirBishopsQueens = them() & (getBb(PieceType::BISHOP) | getBb(PieceType::QUEEN));
        u64 theirRooksQueens   = them() & (getBb(PieceType::ROOK)   | getBb(PieceType::QUEEN));

        u64 potentialAttackers = theirBishopsQueens & attacks::bishopAttacks(kingSquare, them());
        potentialAttackers    |= theirRooksQueens   & attacks::rookAttacks(kingSquare, them());  
    
        u64 pinnedBitboard = 0;

        while (potentialAttackers > 0) {
            Square attackerSquare = poplsb(potentialAttackers);
            u64 maybePinned = us() & BETWEEN[attackerSquare][kingSquare];

            if (std::popcount(maybePinned) == 1)
                pinnedBitboard |= maybePinned;
        }

        return pinnedBitboard;
     }

    inline Move uciToMove(std::string uciMove)
    {
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

    inline void makeMove(std::string uciMove) {
        makeMove(uciToMove(uciMove));
    }

    inline void makeMove(Move move)
    {
        assert(mStates.size() >= 1 && mState == &mStates.back());
        BoardState copy = *mState;
        mStates.push_back(copy);
        mState = &mStates.back();

        Color oppSide = this->oppSide();
        mState->lastMove = move;

        if (move == MOVE_NONE) {
            assert(!inCheck());

            mState->colorToMove = oppSide;
            mState->zobristHash ^= ZOBRIST_COLOR;

            if (mState->enPassantSquare != SQUARE_NONE)
            {
                mState->zobristHash ^= ZOBRIST_FILES[(int)squareFile(mState->enPassantSquare)];
                mState->enPassantSquare = SQUARE_NONE;
            }

            mState->pliesSincePawnOrCapture++;

            if (sideToMove() == Color::WHITE)
                mState->currentMoveCounter++;

            mState->captured = PieceType::NONE;
            return;
        }

        Square from = move.from();
        Square to = move.to();
        auto moveFlag = move.flag();
        PieceType promotion = move.promotion();
        PieceType pieceType = move.pieceType();
        Square capturedPieceSquare = to;

        removePiece(sideToMove(), pieceType, from);

        if (moveFlag == Move::CASTLING_FLAG)
        {
            placePiece(sideToMove(), PieceType::KING, to);
            auto [rookFrom, rookTo] = CASTLING_ROOK_FROM_TO[to];
            removePiece(sideToMove(), PieceType::ROOK, rookFrom);
            placePiece(sideToMove(), PieceType::ROOK, rookTo);
            mState->captured = PieceType::NONE;
        }
        else if (moveFlag == Move::EN_PASSANT_FLAG)
        {
            capturedPieceSquare = sideToMove() == Color::WHITE ? to - 8 : to + 8;
            removePiece(oppSide, PieceType::PAWN, capturedPieceSquare);
            placePiece(sideToMove(), PieceType::PAWN, to);
            mState->captured = PieceType::PAWN;
        }
        else {
            if ((mState->captured = pieceTypeAt(to)) != PieceType::NONE)
                removePiece(oppSide, mState->captured, to);

            placePiece(sideToMove(), 
                       promotion != PieceType::NONE ? promotion : pieceType, 
                       to);
        }

        mState->zobristHash ^= mState->castlingRights; // XOR old castling rights out

        // Update castling rights
        if (pieceType == PieceType::KING)
        {
            mState->castlingRights &= ~CASTLING_MASKS[(int)sideToMove()][CASTLE_SHORT]; 
            mState->castlingRights &= ~CASTLING_MASKS[(int)sideToMove()][CASTLE_LONG]; 
        }
        else if ((1ULL << from) & mState->castlingRights)
            mState->castlingRights &= ~(1ULL << from);
        if ((1ULL << to) & mState->castlingRights)
            mState->castlingRights &= ~(1ULL << to); 

        mState->zobristHash ^= mState->castlingRights; // XOR new castling rights in

        // Update en passant square
        if (mState->enPassantSquare != SQUARE_NONE)
        {
            mState->zobristHash ^= ZOBRIST_FILES[(int)squareFile(mState->enPassantSquare)];
            mState->enPassantSquare = SQUARE_NONE;
        }
        if (moveFlag == Move::PAWN_TWO_UP_FLAG)
        { 
            mState->enPassantSquare = sideToMove() == Color::WHITE ? to - 8 : to + 8;
            mState->zobristHash ^= ZOBRIST_FILES[(int)squareFile(mState->enPassantSquare)];
        }

        mState->colorToMove = oppSide;
        mState->zobristHash ^= ZOBRIST_COLOR;

        if (pieceType == PieceType::PAWN || mState->captured != PieceType::NONE)
            mState->pliesSincePawnOrCapture = 0;
        else
            mState->pliesSincePawnOrCapture++;

        if (sideToMove() == Color::WHITE)
            mState->currentMoveCounter++;

        Square kingSquare = lsb(getBb(sideToMove(), PieceType::KING));
        mState->checkers = attackers(kingSquare) & them();
    }

    inline void undoMove()
    {
        assert(mStates.size() >= 2 && mState == &mStates.back());
        mStates.pop_back();
        mState = &mStates.back();
    }

    inline void pseudolegalMoves(ArrayVec<Move, 256> &moves, bool noisyOnly = false, bool underpromotions = true)
    {
        moves.clear();

        Color enemyColor = oppSide();

        u64 occ = occupancy(),
            ourPawns   = getBb(sideToMove(), PieceType::PAWN),
            ourKnights = getBb(sideToMove(), PieceType::KNIGHT),
            ourBishops = getBb(sideToMove(), PieceType::BISHOP),
            ourRooks   = getBb(sideToMove(), PieceType::ROOK),
            ourQueens  = getBb(sideToMove(), PieceType::QUEEN);

        // En passant
        if (mState->enPassantSquare != SQUARE_NONE)
        {   
            u64 ourEnPassantPawns = attacks::pawnAttacks(mState->enPassantSquare, enemyColor) & ourPawns;

            while (ourEnPassantPawns > 0) {
                Square ourPawnSquare = poplsb(ourEnPassantPawns);
                moves.push_back(Move(ourPawnSquare, mState->enPassantSquare, Move::EN_PASSANT_FLAG));
            }
        }

        while (ourPawns > 0) {
            Square sq = poplsb(ourPawns);
            bool pawnHasntMoved = false, willPromote = false;
            Rank rank = squareRank(sq);

            if (rank == Rank::RANK_2) {
                pawnHasntMoved = sideToMove() == Color::WHITE;
                willPromote = sideToMove() == Color::BLACK;
            } else if (rank == Rank::RANK_7) {
                pawnHasntMoved = sideToMove() == Color::BLACK;
                willPromote = sideToMove() == Color::WHITE;
            }

            // Generate this pawn's captures

            u64 pawnAttacks = attacks::pawnAttacks(sq, sideToMove()) & them();

            while (pawnAttacks > 0) {
                Square targetSquare = poplsb(pawnAttacks);
                if (willPromote) 
                    addPromotions(moves, sq, targetSquare, underpromotions);
                else 
                    moves.push_back(Move(sq, targetSquare, Move::PAWN_FLAG));
            }

            Square squareOneUp = sideToMove() == Color::WHITE ? sq + 8 : sq - 8;
            if (isOccupied(squareOneUp))
                continue;

            if (willPromote) {
                addPromotions(moves, sq, squareOneUp, underpromotions);
                continue;
            }

            if (noisyOnly) continue;

            // pawn 1 square up
            moves.push_back(Move(sq, squareOneUp, Move::PAWN_FLAG));

            // pawn 2 squares up
            Square squareTwoUp = sideToMove() == Color::WHITE ? sq + 16 : sq - 16;
            if (pawnHasntMoved && !isOccupied(squareTwoUp))
                moves.push_back(Move(sq, squareTwoUp, Move::PAWN_TWO_UP_FLAG));
        }

        u64 mask = noisyOnly ? them() : ~us();

        while (ourKnights > 0) {
            Square sq = poplsb(ourKnights);
            u64 knightMoves = attacks::knightAttacks(sq) & mask;
            while (knightMoves > 0) {
                Square targetSquare = poplsb(knightMoves);
                moves.push_back(Move(sq, targetSquare, Move::KNIGHT_FLAG));
            }
        }
        
        while (ourBishops > 0) {
            Square sq = poplsb(ourBishops);
            u64 bishopMoves = attacks::bishopAttacks(sq, occ) & mask;
            while (bishopMoves > 0) {
                Square targetSquare = poplsb(bishopMoves);
                moves.push_back(Move(sq, targetSquare, Move::BISHOP_FLAG));
            }
        }

        while (ourRooks > 0) {
            Square sq = poplsb(ourRooks);
            u64 rookMoves = attacks::rookAttacks(sq, occ) & mask;
            while (rookMoves > 0) {
                Square targetSquare = poplsb(rookMoves);
                moves.push_back(Move(sq, targetSquare, Move::ROOK_FLAG));
            }
        }

        while (ourQueens > 0) {
            Square sq = poplsb(ourQueens);
            u64 queenMoves = attacks::queenAttacks(sq, occ) & mask;
            while (queenMoves > 0) {
                Square targetSquare = poplsb(queenMoves);
                moves.push_back(Move(sq, targetSquare, Move::QUEEN_FLAG));
            }
        }

        Square kingSquare = lsb(getBb(sideToMove(), PieceType::KING));
        u64 kingMoves = attacks::kingAttacks(kingSquare) & mask;

        while (kingMoves > 0) {
            Square targetSquare = poplsb(kingMoves);
            moves.push_back(Move(kingSquare, targetSquare, Move::KING_FLAG));
        }

        // Castling
        if (!noisyOnly && !inCheck())
        {
            // Short castle
            if ((mState->castlingRights & CASTLING_MASKS[(int)sideToMove()][CASTLE_SHORT])
            && !(occ & BETWEEN[kingSquare][kingSquare+3]))
                moves.push_back(Move(kingSquare, kingSquare + 2, Move::CASTLING_FLAG));

            // Long castle
            if ((mState->castlingRights & CASTLING_MASKS[(int)sideToMove()][CASTLE_LONG])
            && !(occ & BETWEEN[kingSquare][kingSquare-4]))
                moves.push_back(Move(kingSquare, kingSquare - 2, Move::CASTLING_FLAG));
        }
    }

    private:

    inline void addPromotions(ArrayVec<Move, 256> &moves, Square sq, Square targetSquare, bool underpromotions)
    {
        moves.push_back(Move(sq, targetSquare, Move::QUEEN_PROMOTION_FLAG));
        if (underpromotions) {
            moves.push_back(Move(sq, targetSquare, Move::ROOK_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::BISHOP_PROMOTION_FLAG));
            moves.push_back(Move(sq, targetSquare, Move::KNIGHT_PROMOTION_FLAG));
        }
    }

    public:

    inline bool isPseudolegalLegal(Move move, u64 pinned) {
        auto moveFlag = move.flag();
        Square from = move.from();
        Square to = move.to();
        Color oppSide = this->oppSide();

        if (moveFlag == Move::CASTLING_FLAG)
            return to > from
                   // Short castle
                   ? !isSquareAttacked(from + 1, oppSide) && !isSquareAttacked(from + 2, oppSide)
                   // Long castle
                   : !isSquareAttacked(from - 1, oppSide) && !isSquareAttacked(from - 2, oppSide);

        Square kingSquare = lsb(getBb(sideToMove(), PieceType::KING));

        if (moveFlag == Move::EN_PASSANT_FLAG) 
        {
            Square capturedSq = sideToMove() == Color::WHITE ? to - 8 : to + 8;
            u64 occAfter = occupancy() ^ (1ULL << from) ^ (1ULL << capturedSq) ^ (1ULL << to);

            u64 bishopsQueens = getBb(PieceType::BISHOP) | getBb(PieceType::QUEEN);
            u64 rooksQueens   = getBb(PieceType::ROOK)   | getBb(PieceType::QUEEN);

            u64 slidingAttackersTo = attacks::bishopAttacks(kingSquare, occAfter) & bishopsQueens;
            slidingAttackersTo    |= attacks::rookAttacks(kingSquare, occAfter)   & rooksQueens;

            return (slidingAttackersTo & them()) == 0;
        }

        if (move.pieceType() == PieceType::KING)
            return !isSquareAttacked(to, oppSide, occupancy() ^ (1ULL << from));

        if (std::popcount(mState->checkers) > 1) 
            return false;

        if ((pinned & (1ULL << from)) > 0 && (LINE_THROUGH[from][to] & (1ULL << kingSquare)) == 0)
            return false;

        // 1 checker
        if (mState->checkers > 0) {
            Square checkerSquare = lsb(mState->checkers);
            return (1ULL << to) & (BETWEEN[kingSquare][checkerSquare] | mState->checkers);
        }

        return true;
    }

    inline bool hasNonPawnMaterial(Color color = Color::NONE)
    {
        if (color == Color::NONE)
            return getBb(PieceType::KNIGHT)    > 0
                   || getBb(PieceType::BISHOP) > 0
                   || getBb(PieceType::ROOK)   > 0
                   || getBb(PieceType::QUEEN)  > 0;

        return getBb(color, PieceType::KNIGHT)    > 0
               || getBb(color, PieceType::BISHOP) > 0
               || getBb(color, PieceType::ROOK)   > 0
               || getBb(color, PieceType::QUEEN)  > 0;
    }

    inline u64 roughHashAfter(Move move) 
    {
        u64 hashAfter = zobristHash() ^ ZOBRIST_COLOR;

        if (move == MOVE_NONE) return hashAfter;
        
        int stm = (int)sideToMove();
        int nstm = (int)oppSide();

        Square to = move.to();
        PieceType captured = pieceTypeAt(to);

        if (captured != PieceType::NONE)
            hashAfter ^= ZOBRIST_PIECES[nstm][(int)captured][to];

        int pieceType = (int)move.pieceType();
        return hashAfter ^ ZOBRIST_PIECES[stm][pieceType][move.from()] ^ ZOBRIST_PIECES[stm][pieceType][to];
    }

    // Cuckoo
    inline bool hasUpcomingRepetition(int ply) {
        //int stateIdxAfterPawnOrCapture = std::max(0, (int)mStates.size() - (int)mState->pliesSincePawnOrCapture - 1);
        //int rootStateIdx = (int)mStates.size() - ply - 1;

        int end = std::min((int)mState->pliesSincePawnOrCapture, (int)mStates.size() - 1);
        if (end < 3) return false;

        u64 occ = occupancy();

        for (int i = 3; i <= end; i += 2)
        {
            assert((int)mStates.size() - 1 - i >= 0);
            u64 moveKey = zobristHash() ^ mStates[(int)mStates.size() - 1 - i].zobristHash;

            int cuckooIdx;

            if (cuckoo::KEYS[cuckooIdx = cuckoo::h1(moveKey)] == moveKey 
            ||  cuckoo::KEYS[cuckooIdx = cuckoo::h2(moveKey)] == moveKey)
            {
                Move move = cuckoo::MOVES[cuckooIdx];
                Square from = move.from();
                Square to = move.to();
                u64 fromBitboard = 1ULL << from;
                u64 toBitboard = 1ULL << to;

                if (((BETWEEN[from][to] | toBitboard) ^ toBitboard) & occ)
                    continue;

                if (ply > i) return true; // Repetition after root

                Color pieceColor = getBb(Color::WHITE) & (occ & fromBitboard ? fromBitboard : toBitboard) 
                                   ? Color::WHITE : Color::BLACK;

                if (pieceColor != sideToMove()) continue;

                // Require one more repetition at and before root
                for (int j = i + 4; j <= end; j += 2)
                    if (mStates[(int)mStates.size() - 1 - i].zobristHash == mStates[(int)mStates.size() - 1 - j].zobristHash)
                        return true;
            }
        }

        return false;
    }

}; // class Board

