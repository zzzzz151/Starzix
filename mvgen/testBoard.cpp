// clang-format off
#include <iostream>
#include <bitset>
#include <string>
#include "move.hpp"
#include "board.hpp"
using namespace std;

int failed = 0, passed = 0;

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

    cout << endl;
}

int main()
{
    cout << "Running board tests..." << endl << endl;

    Board::initZobrist();

    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");

    test("fen1", board.getFen(), START_FEN);
    test("fen2",  board2.getFen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    test("fen3", board3.getFen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");

    bitset<64> got(board.occupied);  
    bitset<64> expected(0xffff00000000ffffULL);  
    test("board.occupied", got, expected);

    test("piecesBitboard(KING) == 2", popcount(board.getPiecesBitboard(PieceType::KING)), 2);
    test("piecesBitboard(KING, WHITE) == 1", popcount(board.getPiecesBitboard(PieceType::KING, WHITE)), 1);

    test("squareFile()", squareFile(33), 'b');
    test("squareFile()", squareFile(15), 'h');

    test("popcount()", popcount((uint64_t)5), 2); // 5 = 101
    test("lsb()", 1ULL << lsb((uint64_t)12), (uint64_t)4); // 12 = 1100

    test("strToSquare()", strToSquare("b7"), (Square)49);

    test("sizeof(Move)", sizeof(Move), 2ULL); // 2 bytes

    Move move = Move(49, 55, PieceType::KNIGHT);
    test("normal move.from", (int)(move.from()), 49);
    test("normal move.to", (int)move.to(), 55);

    move = Move("e1", "g1", PieceType::KING);
    test("castling move.from", squareToStr[(int)move.from()], (string)"e1");
    test("castling move.to", squareToStr[(int)move.to()], (string)"g1");
    test("move.flag=castling", move.getTypeFlag(), move.CASTLING_FLAG);

    
    board = Board("rnb1kbnr/1P1pqppp/4p3/2p5/8/8/P1PPPPPP/RNBQKBNR w KQkq - 1 5");
    move = Move::fromUci("b7c8b", board.pieces);
    test("Move::fromUci() == move.toUci()", move.toUci(), (string)"b7c8b");
    test("move.BISHOP_PROMOTION_FLAG", (int)move.getTypeFlag(), (int)move.BISHOP_PROMOTION_FLAG);
    test("move.getPromotionPieceType() == bishop", (int)move.getPromotionPieceType(), (int)PieceType::BISHOP);

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 0 9");
    // moves a2b1q e1g1 g7g5 h5g6 f6g4
    board.makeMove(Move::fromUci("a2b1q", board.pieces)); // queen promotion by black
    board.makeMove(Move::fromUci("e1g1", board.pieces)); // white castles short
    board.makeMove(Move::fromUci("g7g5", board.pieces)); // black allows enpassant
    board.makeMove(Move::fromUci("h5g6", board.pieces)); // white takes on passant
    board.makeMove(Move::fromUci("f6g4", board.pieces)); // black captures white pawn with black knight
    board.makeMove(Move::fromUci("a1a2", board.pieces)); // white moves rook up (test 50move counter)
    test("makeMove()", board.getFen(), (string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");

    test("zobrist", board.getZobristHash(), Board("rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12").getZobristHash());

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed << endl;

    return 0;
}

