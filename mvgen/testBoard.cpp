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

inline void printMoves(Move* moves, int size)
{
    for (int i = 0; i < size; i++)
        cout << moves[i].toUci() << (i != size - 1 ? ", " : "");
    cout << endl;
}

uint64_t perft(Board board, int depth)
{
    Move moves[218];
    int numMoves = board.getMoves(moves);

    if (depth == 1) return numMoves;
    if (depth == 0) return 1;
        
    uint64_t nodes = 0;

    for (int i = 0; i < numMoves; i++) {
        board.makeMove(moves[i]);
        nodes += perft(board, depth - 1);
        board.undoMove(moves[i]);
    }

    return nodes;
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

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove(Move::fromUci("g7g5", board.pieces));
    test("makeMove() creates en passant", board.getFen(), (string)"rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");

    board = Board("rnbqkbnr/pppppp1p/8/8/1P4pP/3P4/P1P1PPP1/RNBQKBNR b KQkq h3 0 3");
    board.makeMove(Move::fromUci("g4h3", board.pieces));
    test("makeMove() creates en passant v2", board.getFen(), (string)"rnbqkbnr/pppppp1p/8/8/1P6/3P3p/P1P1PPP1/RNBQKBNR w KQkq - 0 4");

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    // queen promotion by black
    Move move1 = Move::fromUci("a2b1q", board.pieces);
    board.makeMove(move1); 
    // white castles short
    Move move2 = Move::fromUci("e1g1", board.pieces);
    board.makeMove(move2); 
    // black allows enpassant
    Move move3 = Move::fromUci("g7g5", board.pieces);
    board.makeMove(move3); 
    // white takes on passant
    Move move4 = Move::fromUci("h5g6", board.pieces);
    board.makeMove(move4); 
    // black captures white pawn with black knight
    Move move5 = Move::fromUci("f6g4", board.pieces);
    board.makeMove(move5); 
    // white moves rook up (test 50move counter)
    Move move6 = Move::fromUci("a1a2", board.pieces);
    board.makeMove(move6); 

    test("makeMove()", board.getFen(), (string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");
    test("zobrist", board.getZobristHash(), Board("rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12").getZobristHash());

    board.undoMove(move6);
    board.undoMove(move5); 
    board.undoMove(move4); 
    board.undoMove(move3); 
    board.undoMove(move2);
    board.undoMove(move1);
    test("undoMove()", board.getFen(), (string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");

    Move moves[218];

    board = Board(START_FEN);
    test("perft(1)", perft(board, 1), 20ULL);
    test("perft(2)", perft(board, 2), 400ULL);
    test("perft(3)", perft(board, 3), 8902ULL);
    //test("perft(4)", perft(board, 4), 197281ULL);
    //test("perft(5)", perft(board, 5), 4865609ULL);
    //test("perft(6)", perft(board, 6), 119060324ULL);

    // kiwipete
    //board = Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    //test("perft(4) kiwipete", perft(board, 4), 4085603ULL);

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed << endl;

    return 0;
}

