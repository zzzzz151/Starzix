// clang-format off
#include <iostream>
#include <bitset>
#include <string>
#include <chrono>
#include "board.hpp"
using namespace std;

int failed = 0, passed = 0;
const string POSITION2_KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const string POSITION3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
const string POSITION4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const string POSITION4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ";
const string POSITION5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ";
const string POSITION6 = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ";

template <typename T> void test(string testName, T got, T expected)
{
    if (expected != got)
    {
        cout << "[FAILED]";
        failed++;
    }
    else 
    {
        cout << "[PASSED]";
        passed++;
    }
    cout << " Test: " << testName << endl;
    if (expected != got) cout << "Expected: " << expected << endl << "Got: " << got << endl;
}

inline uint64_t perft(Board &board, int depth, bool capturesOnly = false)
{
    if (depth == 0) return (capturesOnly ? 0 : 1);

    MovesList moves = board.pseudolegalMoves(capturesOnly);        
    uint64_t nodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i])) continue;
        nodes += perft(board, depth - 1, capturesOnly);
        board.undoMove(moves[i]);
    }

    return nodes;
}

inline void perftDivide(Board board, int depth, bool capturesOnly = false)
{
    MovesList moves = board.pseudolegalMoves(capturesOnly); 

    if (moves.size() == 0)
         return;

    uint64_t totalNodes = 0;
    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i])) continue;
        uint64_t nodes = perft(board, depth - 1, capturesOnly);
        cout << moves[i].toUci() << ": " << nodes << endl;
        totalNodes += nodes;
        board.undoMove(moves[i]);
    }

    cout << "Total: " << totalNodes << endl;
}

inline void perftBench(Board &board, int depth, bool capturesOnly = false)
{
    chrono::steady_clock::time_point start =  chrono::steady_clock::now();
    uint64_t nodes = perft(board, depth, capturesOnly);
    int millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
    uint64_t nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1) * 1000;
    cout << "perft depth " << depth << " time " << millisecondsElapsed << " nodes " << nodes << " nps " << nps << " fen " << board.fen() << endl;
}


inline void printLegalMoves(Board board)
{
    MovesList moves = board.pseudolegalMoves();
    int castles = 0;
    int promotions = 0;
    int normals = 0;
    int enPassants = 0;
    int illegals = 0;

    uint64_t a = board.zobristHash();

    for (int i = 0; i < moves.size(); i++)
    {
        if (!board.makeMove(moves[i])) 
        {
            illegals++;
            cout << "Skipping illegal move " << moves[i].toUci() << endl;
            continue;
        }
        board.undoMove(moves[i]);
        cout << moves[i].toUci() << endl;
        uint16_t flag = moves[i].typeFlag();
        if (flag == Move::CASTLING_FLAG)
            castles++;
        else if (flag == Move::NORMAL_FLAG)
            normals++;
        else if (flag == Move::EN_PASSANT_FLAG)
            enPassants++;
        else if (moves[i].promotionPieceType() != PieceType::NONE)
            promotions++;
    }

    cout << "Normals: " << normals << endl;
    cout << "Castles: " << castles << endl;
    cout << "Promotions: " << promotions << endl;
    cout << "En passants: " << enPassants << endl;
    cout << "Illegals: " << illegals << endl;
    cout << endl;
}

int main()
{
    Board::initZobrist();
    initMoves();

    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");

    test("fen1", board.fen(), START_FEN);
    test("fen2",  board2.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    test("fen3", board3.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");

    bitset<64> got(board.occupancy());  
    bitset<64> expected(0xffff00000000ffffULL);  
    test("board.occupancy()", got, expected);

    test("pieceBitboard(KING) == 2", popcount(board.pieceBitboard(PieceType::KING)), 2);
    test("pieceBitboard(KING, WHITE) == 1", popcount(board.pieceBitboard(PieceType::KING, WHITE)), 1);

    test("squareFile()", squareFile(33), 'b');
    test("squareFile()", squareFile(15), 'h');

    test("isEdgeSquare()", isEdgeSquare(19), false);

    test("popcount()", popcount((uint64_t)5), 2); // 5 = 101
    test("lsb()", 1ULL << lsb((uint64_t)12), (uint64_t)4); // 12 = 1100

    test("strToSquare()", strToSquare("b7"), (Square)49);

    test("sizeof(Move)", sizeof(Move), 2ULL); // 2 bytes

    Move move = Move(49, 55, Move::NORMAL_FLAG);
    test("normal move.from", (int)(move.from()), 49);
    test("normal move.to", (int)move.to(), 55);

    move = Move("e1", "g1", Move::CASTLING_FLAG);
    test("castling move.from", squareToStr[(int)move.from()], (string)"e1");
    test("castling move.to", squareToStr[(int)move.to()], (string)"g1");
    test("move.flag=castling", move.typeFlag(), move.CASTLING_FLAG);

    
    board = Board("rnb1kbnr/1P1pqppp/4p3/2p5/8/8/P1PPPPPP/RNBQKBNR w KQkq - 1 5");
    move = Move::fromUci("b7c8b", board.piecesBySquare());
    test("Move::fromUci() == move.toUci()", move.toUci(), (string)"b7c8b");
    test("move.BISHOP_PROMOTION_FLAG", (int)move.typeFlag(), (int)move.BISHOP_PROMOTION_FLAG);
    test("move.promotionPieceType() == bishop", (int)move.promotionPieceType(), (int)PieceType::BISHOP);

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove(Move::fromUci("g7g5", board.piecesBySquare()));
    test("makeMove() creates en passant", board.fen(), (string)"rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");

    board = Board("rnbqkbnr/pppppp1p/8/8/1P4pP/3P4/P1P1PPP1/RNBQKBNR b KQkq h3 0 3");
    board.makeMove(Move::fromUci("g4h3", board.piecesBySquare()));
    test("makeMove() creates en passant v2", board.fen(), (string)"rnbqkbnr/pppppp1p/8/8/1P6/3P3p/P1P1PPP1/RNBQKBNR w KQkq - 0 4");

    board = Board(START_FEN);
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("e2e4", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("d7d5", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("b1c3", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("b8c6", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("c3b1", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove(Move::fromUci("c6b8", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), true);
    board.makeMove(Move::fromUci("g1h3", board.piecesBySquare()));
    test("isRepetition()", board.isRepetition(), false);

    board = Board(POSITION2_KIWIPETE); 
    test("isSquareAttacked() #1", board.isSquareAttacked(strToSquare("c3")), true);
    test("isSquareAttacked() #2", board.isSquareAttacked(strToSquare("f3")), false);
    test("isSquareAttacked() #3", board.isSquareAttacked(strToSquare("e1")), false);
    test("isSquareAttacked() #4", board.isSquareAttacked(strToSquare("e5")), false);
    test("isSquareAttacked() #5", board.isSquareAttacked(strToSquare("d5")), true);
    test("isSquareAttacked() #6", board.isSquareAttacked(strToSquare("e2")), true);
    test("isSquareAttacked() #7 (empty sq attacked)", board.isSquareAttacked(strToSquare("a3")), true);
    test("isSquareAttacked() #8 (empty sq not attacked)", board.isSquareAttacked(strToSquare("b3")), false);
    board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ");
    test("isSquareAttacked() #9", board.isSquareAttacked(strToSquare("a5")), false);

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    uint64_t zobristHashBefore = board.zobristHash();

    // queen promotion by black
    Move move1 = Move::fromUci("a2b1q", board.piecesBySquare());
    board.makeMove(move1); 
    // white castles short
    Move move2 = Move::fromUci("e1g1", board.piecesBySquare());
    board.makeMove(move2); 
    // black allows enpassant
    Move move3 = Move::fromUci("g7g5", board.piecesBySquare());
    board.makeMove(move3); 
    // white takes on passant
    Move move4 = Move::fromUci("h5g6", board.piecesBySquare());
    board.makeMove(move4); 
    // black captures white pawn with black knight
    Move move5 = Move::fromUci("f6g4", board.piecesBySquare());
    board.makeMove(move5); 
    // white moves rook up (test 50move counter)
    Move move6 = Move::fromUci("a1a2", board.piecesBySquare());
    board.makeMove(move6); 

    test("makeMove()", board.fen(), (string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");
    test("zobristHash is what we expect after making moves", board.zobristHash(), Board("rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12").zobristHash());

    board.undoMove(move6);
    board.undoMove(move5); 
    board.undoMove(move4); 
    board.undoMove(move3); 
    board.undoMove(move2);
    board.undoMove(move1);
    test("undoMove()", board.fen(), (string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    test("zobristHash is same after making and undoing moves", board.zobristHash(), zobristHashBefore);

    Board boardPos2 = Board(POSITION2_KIWIPETE);
    Board boardPos3 = Board(POSITION3);
    Board boardPos4 = Board(POSITION4);
    Board boardPos4Mirrored = Board(POSITION4_MIRRORED);
    Board boardPos5 = Board(POSITION5);
    Board boardpos6 = Board(POSITION6);

    
    test("perft(1) position 2 kiwipete", perft(boardPos2, 1), 48ULL);
    test("perft(1) position 3", perft(boardPos3, 1), 14ULL);
    test("perft(1) position 4", perft(boardPos4, 1), 6ULL);
    test("perft(1) position 4 mirrored", perft(boardPos4Mirrored, 1), 6ULL);
    test("perft(1) position 5", perft(boardPos5, 1), 44ULL);
    test("perft(1) position 6", perft(boardpos6, 1), 46ULL);
    test("perft(5) position 2 kiwipete", perft(boardPos2, 5), 193690690ULL);

    board = Board(START_FEN);
    test("perft(0)", perft(board, 0), 1ULL);
    test("perft(1)", perft(board, 1), 20ULL);
    test("perft(2)", perft(board, 2), 400ULL);
    test("perft(3)", perft(board, 3), 8902ULL);
    test("perft(4)", perft(board, 4), 197281ULL);
    test("perft(5)", perft(board, 5), 4865609ULL);
    test("perft(6)", perft(board, 6), 119060324ULL);

    test("perft(0) captures", perft(board, 0, true), 0ULL);
    test("perft(1) captures", perft(board, 1, true), 0ULL);
    test("perft(2) captures", perft(board, 2, true), 0ULL);
    test("perft(3) captures", perft(board, 3, true), 34ULL);

    perftBench(board, 6);
    

    /*
    boardPos2.makeMove(Move::fromUci("a1b1", board.piecesBySquare()));
    boardPos2.makeMove(Move::fromUci("b4c3", board.piecesBySquare()));
    boardPos2.makeMove(Move::fromUci("b1a1", board.piecesBySquare()));
    boardPos2.makeMove(Move::fromUci("c3b2", board.piecesBySquare()));
    cout << "--------------" << endl;
    perftDivide(boardPos2, 1);
    */

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed;

    return 0;
}

