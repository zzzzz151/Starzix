// clang-format off
#include "../src-test/board.hpp"
#include "../src-test/perft.hpp"
using namespace std;

int failed = 0, passed = 0;
const string POSITION2_KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const string POSITION3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
const string POSITION4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const string POSITION4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ";
const string POSITION5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ";

template <typename T> inline void test(string testName, T got, T expected)
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

inline void test(string testName, File got, File expected)
{
    return test(testName, (int)got, int(expected));
}

inline void test(string testName, Rank got, Rank expected)
{
    return test(testName, (int)got, int(expected));
}

template <typename T> inline void testNotEquals(string testName, T got, T expected)
{
    if (expected == got)
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
    if (expected == got) cout << "Expected equal but theyre different! Expected: " << expected << endl;
}

int main()
{   
    Board::initZobrist();
    attacks::initAttacks();
    //nnue::loadNetFromFile();

    test("popcount()", popcount((uint64_t)5), 2); // 5 = 101
    test("lsb()", 1ULL << lsb((uint64_t)12), (uint64_t)4); // 12 = 1100

    test("strToSquare()", strToSquare("b7"), (Square)49);

    test("squareFile() A", squareFile(8), File::A);
    test("squareFile() B", squareFile(9), File::B);
    test("squareFile() C", squareFile(10), File::C);
    test("squareFile() D", squareFile(11), File::D);
    test("squareFile() E", squareFile(12), File::E);
    test("squareFile() F", squareFile(13), File::F);
    test("squareFile() G", squareFile(14), File::G);
    test("squareFile() H", squareFile(15), File::H);

    test("squareRank() 1", squareRank(7), Rank::RANK_1);
    test("squareRank() 2", squareRank(1+8*1), Rank::RANK_2);
    test("squareRank() 3", squareRank(1+8*2), Rank::RANK_3);
    test("squareRank() 4", squareRank(1+8*3), Rank::RANK_4);
    test("squareRank() 5", squareRank(1+8*4), Rank::RANK_5);
    test("squareRank() 6", squareRank(1+8*5), Rank::RANK_6);
    test("squareRank() 7", squareRank(1+8*6), Rank::RANK_7);
    test("squareRank() 8", squareRank(1+8*7), Rank::RANK_8);

    test("sizeof(Move)", sizeof(Move), 2ULL); // 2 bytes

    Move move = Move(49, 55, Move::NORMAL_FLAG);
    test("normal move.from", (int)(move.from()), 49);
    test("normal move.to", (int)move.to(), 55);

    move = Move(strToSquare("e1"), strToSquare("g1"), Move::CASTLING_FLAG);
    test("castling move.from", SQUARE_TO_STR[(int)move.from()], (string)"e1");
    test("castling move.to", SQUARE_TO_STR[(int)move.to()], (string)"g1");
    test("move.flag=castling", move.typeFlag(), move.CASTLING_FLAG);

    move = Move(strToSquare("b7"), strToSquare("c8"), Move::BISHOP_PROMOTION_FLAG); // b7c8b
    test("move.toUci()", move.toUci(), (string)"b7c8b");
    test("move.BISHOP_PROMOTION_FLAG", (int)move.typeFlag(), (int)move.BISHOP_PROMOTION_FLAG);
    test("move.promotionPieceType() == bishop", (int)move.promotionPieceType(), (int)PieceType::BISHOP);
    
    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");
    
    test("fen1", board.fen(), START_FEN);
    test("fen2",  board2.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    test("fen3", board3.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");
    
    // Test occupancy on start pos
    bitset<64> got(board.occupancy());  
    bitset<64> expected(0xffff00000000ffffULL);  
    test("board.occupancy()", got, expected);
    test("getBitboard(KING) == 2", popcount(board.getBitboard(PieceType::KING)), 2);
    test("getBitboard(KING, WHITE) == 1", popcount(board.getBitboard(PieceType::KING, Color::WHITE)), 1);

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    test("move has PAWN_TWO_UP_FLAG flag", Move::fromUci("g7g5", board.getPieces()).typeFlag(), Move::PAWN_TWO_UP_FLAG);
    board.makeMove("g7g5");
    test("makeMove() creates en passant", board.fen(), (string)"rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");

    board = Board("rnbqkbnr/pppppp1p/8/8/1P4pP/3P4/P1P1PPP1/RNBQKBNR b KQkq h3 0 3");
    board.makeMove("g4h3");
    test("fen is correct after taking en passant", board.fen(), (string)"rnbqkbnr/pppppp1p/8/8/1P6/3P3p/P1P1PPP1/RNBQKBNR w KQkq - 0 4");
    
    board = Board(START_FEN);
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("e2e4");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("d7d5");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("b1c3");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("b8c6");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("c3b1");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("c6b8");
    test("isRepetition()", board.isRepetition(), true);
    board.makeMove("g1h3");
    test("isRepetition()", board.isRepetition(), false);

    board = Board(POSITION2_KIWIPETE); 
    test("isSquareAttacked() #1 (empty sq attacked by us)", board.isSquareAttacked(strToSquare("e3"), board.sideToMove()), true);
    test("isSquareAttacked() #2 (empty sq not attacked by us)", board.isSquareAttacked(strToSquare("a5"), board.sideToMove()), false);
    test("isSquareAttacked() #3 (empty sq attacked by enemy)", board.isSquareAttacked(strToSquare("d6"), board.oppSide()), true);
    test("isSquareAttacked() #4 (empty sq not attacked by enemy)", board.isSquareAttacked(strToSquare("b3"), board.oppSide()), false);
    test("isSquareAttacked() #5 (we can capture enemy in this square)", board.isSquareAttacked(strToSquare("d7"), board.sideToMove()), true);
    test("isSquareAttacked() #6 (we cannot capture enemy in this square)", board.isSquareAttacked(strToSquare("b4"), board.sideToMove()), false);
    test("isSquareAttacked() #7 (enemy can capture us in this square)", board.isSquareAttacked(strToSquare("e2"), board.oppSide()), true);
    test("isSquareAttacked() #8 (enemy cannot capture us in this square)", board.isSquareAttacked(strToSquare("h2"), board.oppSide()), false);

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    test("inCheck() returns false", board.inCheck(), false);

    board = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    if (!board.inCheck())
    {
        board.makeNullMove();
        test("makeNullMove() fen", board.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk - 1 15");
        board.undoNullMove();
        test("undoNullMove() fen", board.fen(), (string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    }

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    uint64_t zobristHashBefore = board.getZobristHash();
    board.makeMove("a2b1q");  // black promotes to queen | rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQK2R w KQkq - 0 10
    board.makeMove("e1g1"); // white castles short | rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10
    board.makeMove("g7g5"); // black allows enpassant | rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11
    board.makeMove("h5g6"); // white takes on passant | rnbqkb1r/4pp1p/1p1p1nP1/2p5/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 0 11
    board.makeMove("f6g4");  // black captures white pawn with black knight | rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/2P2P2/RqBQ1RK1 w kq - 0 12
    board.makeMove("a1a2"); // white moves rook up (test 50move counter) | rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12
    test("fen is what we expect after 6x makeMove()", board.fen(), (string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");
    test("zobristHash is what we expect after making moves", board.getZobristHash(), Board("rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12").getZobristHash());

    for (int i = 5; i >= 0; i--)
        board.undoMove();
    test("fen is what we expect after 6x undoMove()", board.fen(), (string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    test("zobristHash is same after making and undoing moves", board.getZobristHash(), zobristHashBefore);

    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    uint64_t zobHash = board.getZobristHash();
    board.makeMove("a8b8"); // black rook moves
    uint64_t zobHash2 = board.getZobristHash();
    testNotEquals("Zobrist hash not equals after losing castle right", zobHash2, zobHash);
    board.undoMove();
    uint64_t zobHash3 = board.getZobristHash();
    testNotEquals("Zobrist hash not equals after undoing move", zobHash3, zobHash2);
    test("Zobrist hash equals after undoing move to before undoing the move", zobHash3, zobHash);

    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    board.makeMove("b7b5"); // creates enpassant target square b6
    zobHash2 = board.getZobristHash();
    testNotEquals("Zobrist hash not equals after creating enpassant sq", zobHash2, zobHash);
    board.undoMove();
    test("Zobrist hash equals after undoing move", board.getZobristHash(), zobHash);

    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    board.makeMove("a8b8");
    board.makeMove("e1e2");
    board.makeMove("d7d5");
    test("Zobrist hash equals (making 3 moves vs from fen)", board.getZobristHash(), Board("1r2k2r/ppp1pppp/8/2PpP2P/8/8/4K3/R6R w k d6 0 3").getZobristHash());

    board = Board("rnb1kbnr/pppp1ppp/8/4p1q1/3P4/1P6/P1P1PPPP/RNBQKBNR w KQkq - 1 3");
    zobHash = board.getZobristHash();
    board.makeMove("e1d2"); // illegal
    test("Zobrist hash equals after illegal move", zobHash, board.getZobristHash());

    board = Board(START_FEN);
    Board boardPos2 = Board(POSITION2_KIWIPETE);
    Board boardPos3 = Board(POSITION3);
    Board boardPos4 = Board(POSITION4);
    Board boardPos4Mirrored = Board(POSITION4_MIRRORED);
    Board boardPos5 = Board(POSITION5);
    board.perft = true;
    boardPos2.perft = true;
    boardPos3.perft = true;
    boardPos4.perft = true;
    boardPos4Mirrored.perft = true;
    boardPos5.perft = true;
    
    test("perft(1) position 2 kiwipete", perft(boardPos2, 1), 48ULL);
    test("perft(1) position 3", perft(boardPos3, 1), 14ULL);
    test("perft(1) position 4", perft(boardPos4, 1), 6ULL);
    test("perft(1) position 4 mirrored", perft(boardPos4Mirrored, 1), 6ULL);
    test("perft(1) position 5", perft(boardPos5, 1), 44ULL);
    test("perft(5) position 2 kiwipete", perft(boardPos2, 5), 193690690ULL);

    test("perft(0)", perft(board, 0), 1ULL);
    test("perft(1)", perft(board, 1), 20ULL);
    test("perft(2)", perft(board, 2), 400ULL);
    test("perft(3)", perft(board, 3), 8902ULL);
    test("perft(4)", perft(board, 4), 197281ULL);
    test("perft(5)", perft(board, 5), 4865609ULL);
    test("perft(6)", perft(board, 6), 119060324ULL);

    test("perft(0) captures", perftCaptures(board, 0), 0ULL);
    test("perft(1) captures", perftCaptures(board, 1), 0ULL);
    test("perft(2) captures", perftCaptures(board, 2), 0ULL);
    test("perft(3) captures", perftCaptures(board, 3), 34ULL);
    test("perft(4) captures", perftCaptures(board, 4), 1576ULL);
    test("perft(5) captures", perftCaptures(board, 5), 82719ULL);
    test("perft(6) captures", perftCaptures(board, 6), 2812008ULL);

    test("perft(0) captures, pos2 kiwipete", perftCaptures(boardPos2, 0), 0ULL);
    test("perft(1) captures, pos2 kiwipete", perftCaptures(boardPos2, 1), 8ULL);
    test("perft(2) captures, pos2 kiwipete", perftCaptures(boardPos2, 2), 351ULL);
    test("perft(3) captures, pos2 kiwipete", perftCaptures(boardPos2, 3), 17102ULL);
    test("perft(4) captures, pos2 kiwipete", perftCaptures(boardPos2, 4), 757163ULL);
    test("perft(5) captures, pos2 kiwipete", perftCaptures(boardPos2, 5), 35043416ULL);

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed << endl;
    
    return 0;
}

