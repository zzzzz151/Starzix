#include <iostream>
#include "board.hpp"
#include "builtin.hpp"
using namespace std;

int failed = 0, passed = 0;

template <typename T> void test(string testName, T got, T expected)
{
    cout << "Test: " << testName << endl;

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
    cout << endl << "Expected: " << expected << endl << "Got: " << got << endl << endl;

}

int main()
{
    cout << "Running board tests..." << endl << endl;

    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");

    test("fen", board.getFen(), START_FEN);
    test("fen",  board2.getFen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    test("fen", board3.getFen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");

    test("occupied", popcount(board.occupied), 16);
    test("pieces bitboard", popcount(board.getPiecesBitboard(PieceType::KING)), 2);
    test("pieces bitboard", popcount(board.getPiecesBitboard(PieceType::KING, WHITE)), 1);

    test("squareFile()", squareFile(33), 'b');
    test("squareFile()", squareFile(15), 'h');

    test("popcount()", popcount((uint64_t)5), 2); // 5 = 101
    test("lsb()", 1ULL << lsb((uint64_t)12), (uint64_t)4); // 12 = 1100

    test("sizeof(Move)", sizeof(Move), 2ULL); // 2 bytes

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed << endl;

    return 0;
}

