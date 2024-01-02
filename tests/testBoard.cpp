// clang-format off
#include "../src-test/board.hpp"
#include "../src-test/perft.hpp"

int failed = 0, passed = 0;
const std::string POSITION2_KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const std::string POSITION3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
const std::string POSITION4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const std::string POSITION4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ";
const std::string POSITION5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ";

template <typename T> inline void test(std::string testName, T got, T expected)
{
    if (expected != got)
    {
        std::cout << "[FAILED]";
        failed++;
    }
    else 
    {
        std::cout << "[PASSED]";
        passed++;
    }
    std::cout << " Test: " << testName << std::endl;
    if (expected != got) std::cout << "Expected: " << expected << std::endl << "Got: " << got << std::endl;
}

inline void test(std::string testName, File got, File expected)
{
    return test(testName, (int)got, int(expected));
}

inline void test(std::string testName, Rank got, Rank expected)
{
    return test(testName, (int)got, int(expected));
}

template <typename T> inline void testNotEquals(std::string testName, T got, T expected)
{
    if (expected == got)
    {
        std::cout << "[FAILED]";
        failed++;
    }
    else 
    {
        std::cout << "[PASSED]";
        passed++;
    }
    std::cout << " Test: " << testName << std::endl;
    if (expected == got) std::cout << "Expected equal but theyre different! Expected: " << expected << std::endl;
}

int main()
{   
    attacks::init();

    test("std::popcount()", std::popcount((uint64_t)5), 2); // 5 = 101
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

    Move move = Move(49, 55, Move::NULL_FLAG);
    test("move.from", (int)(move.from()), 49);
    test("move.to", (int)move.to(), 55);

    move = Move(strToSquare("e1"), strToSquare("g1"), Move::CASTLING_FLAG);
    test("castling move.from", SQUARE_TO_STR[(int)move.from()], (std::string)"e1");
    test("castling move.to", SQUARE_TO_STR[(int)move.to()], (std::string)"g1");
    test("move.flag=castling", move.flag(), Move::CASTLING_FLAG);

    move = Move(strToSquare("b7"), strToSquare("c8"), Move::BISHOP_PROMOTION_FLAG); // b7c8b
    test("move.toUci()", move.toUci(), (std::string)"b7c8b");
    test("move.BISHOP_PROMOTION_FLAG", (int)move.flag(), (int)move.BISHOP_PROMOTION_FLAG);
    test("move.promotion() == bishop", (int)move.promotion(), (int)PieceType::BISHOP);
    
    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");
    
    test("fen1", board.fen(), START_FEN);
    test("fen2",  board2.fen(), (std::string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    test("fen3", board3.fen(), (std::string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");
    
    // Test occupancy on start pos
    std::bitset<64> got(board.occupancy());  
    std::bitset<64> expected(0xffff00000000ffffULL);  
    test("board.occupancy()", got, expected);
    test("bitboard(KING) == 2", std::popcount(board.bitboard(PieceType::KING)), 2);
    test("bitboard(WHITE, KING) == 1", std::popcount(board.bitboard(Color::WHITE, PieceType::KING)), 1);

    // Test pawn 2 up and en passant
    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove("g7g5");
    test("makeMove() creates en passant", 
         board.fen(), 
         (std::string)"rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");

    // Test en passant
    board = Board("rnbqkbnr/pppppp1p/8/8/1P4pP/3P4/P1P1PPP1/RNBQKBNR b KQkq h3 0 3");
    board.makeMove("g4h3");
    test("fen is correct after taking en passant", 
         board.fen(), 
         (std::string)"rnbqkbnr/pppppp1p/8/8/1P6/3P3p/P1P1PPP1/RNBQKBNR w KQkq - 0 4");
    
    // Test position repetition
    board = Board(START_FEN);
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("g1f3");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("g8f6");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("f3g1");
    test("isRepetition()", board.isRepetition(), false);
    board.makeMove("f6g8");
    test("isRepetition()", board.isRepetition(), true);
    board.makeMove("b1c3");
    test("isRepetition()", board.isRepetition(), false);
    board.undoMove();
    test("isRepetition()", board.isRepetition(), true);
    board.undoMove();
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

    // Test inCheck()
    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    test("inCheck() returns false", board.inCheck(), false);

    // Test null move
    board = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    if (!board.inCheck())
    {
        board.makeMove(MOVE_NONE);
        test("board.makeMove(MOVE_NONE)) fen", 
             board.fen(), 
             (std::string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk - 1 15");
        board.undoMove();
        test("undo null move fen", 
             board.fen(), 
             (std::string)"1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    }

    // Test making moves and zobrist hash

    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    uint64_t zobristHashBefore = board.zobristHash();

    board.makeMove("a2b1q"); // black promotes to queen | rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQK2R w KQkq - 0 10
    test("fen after a2b1q", board.fen(), (std::string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQK2R w KQkq - 0 10");
    board.makeMove("e1g1");  // white castles short | rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10
    test("fen after e1g1", board.fen(), (std::string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove("g7g5");  // black allows enpassant | rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11
    test("fen after g7g5", board.fen(), (std::string)"rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    board.makeMove("h5g6");  // white takes on passant | rnbqkb1r/4pp1p/1p1p1nP1/2p5/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 0 11
    test("fen after h5g6", board.fen(), (std::string)"rnbqkb1r/4pp1p/1p1p1nP1/2p5/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 0 11");
    board.makeMove("f6g4");  // black captures white pawn with black knight | rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/2P2P2/RqBQ1RK1 w kq - 0 12
    test("fen after f6g4", board.fen(), (std::string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/2P2P2/RqBQ1RK1 w kq - 0 12");
    board.makeMove("a1a2");  // white moves rook up (test 50move counter) | rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12
    test("fen after a1a2", board.fen(), (std::string)"rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");

    std::string expectedFen = "rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12";
    test("fen is what we expect after making moves", board.fen(), expectedFen);
    test("zobristHash is what we expect after making moves", 
         board.zobristHash(), 
         Board(expectedFen).zobristHash());

    // Test undoing moves and zobrist hash
    for (int i = 5; i >= 0; i--)
        board.undoMove();
    test("fen is what we expect after 6x undoMove()", 
         board.fen(), 
         (std::string)"rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    test("zobristHash is same after making and undoing moves", board.zobristHash(), zobristHashBefore);

    // Test zobrist hash
    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    uint64_t zobHash = board.zobristHash();
    board.makeMove("a8b8"); // black rook moves
    uint64_t zobHash2 = board.zobristHash();
    testNotEquals("Zobrist hash not equals after losing castle right", zobHash2, zobHash);
    board.undoMove();
    uint64_t zobHash3 = board.zobristHash();
    testNotEquals("Zobrist hash not equals after undoing move", zobHash3, zobHash2);
    test("Zobrist hash equals after undoing move to before undoing the move", zobHash3, zobHash);

    // Test en passant and zobrist hash
    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    board.makeMove("b7b5"); // creates enpassant target square b6
    zobHash2 = board.zobristHash();
    testNotEquals("Zobrist hash not equals after creating enpassant sq", zobHash2, zobHash);
    board.undoMove();
    test("Zobrist hash equals after undoing move", board.zobristHash(), zobHash);

    // Test zobrist hash
    board = Board("r3k2r/pppppppp/8/2P1P2P/8/8/8/R3K2R b KQkq - 0 1");
    board.makeMove("a8b8");
    board.makeMove("e1e2");
    board.makeMove("d7d5");
    test("Zobrist hash equals (making 3 moves vs from fen)", 
        board.zobristHash(), 
        Board("1r2k2r/ppp1pppp/8/2PpP2P/8/8/4K3/R6R w k d6 0 3").zobristHash());

    // Test illegal move
    board = Board("rnb1kbnr/pppp1ppp/8/4p1q1/3P4/1P6/P1P1PPPP/RNBQKBNR w KQkq - 1 3");
    zobHash = board.zobristHash();
    test("makeMove() returns false if illegal", board.makeMove("e1d2"), false);
    test("Zobrist hash equals after illegal move", zobHash, board.zobristHash());

    // Perft's

    board = Board(START_FEN);
    Board boardPos2 = Board(POSITION2_KIWIPETE);
    Board boardPos3 = Board(POSITION3);
    Board boardPos4 = Board(POSITION4);
    Board boardPos4Mirrored = Board(POSITION4_MIRRORED);
    Board boardPos5 = Board(POSITION5);

    test("perft(0) startpos", perft::perftBench(board, 0), 1ULL);
    test("perft(1) startpos", perft::perftBench(board, 1), 20ULL);
    test("perft(2) startpos", perft::perftBench(board, 2), 400ULL);
    test("perft(3) startpos", perft::perftBench(board, 3), 8902ULL);
    test("perft(4) startpos", perft::perftBench(board, 4), 197281ULL);
    test("perft(5) startpos", perft::perftBench(board, 5), 4865609ULL);
    test("perft(6) startpos", perft::perftBench(board, 6), 119060324ULL);

    test("perft(1) position 2 kiwipete", perft::perftBench(boardPos2, 1), 48ULL);
    test("perft(1) position 3", perft::perftBench(boardPos3, 1), 14ULL);
    test("perft(1) position 4", perft::perftBench(boardPos4, 1), 6ULL);
    test("perft(1) position 4 mirrored", perft::perftBench(boardPos4Mirrored, 1), 6ULL);
    test("perft(1) position 5", perft::perftBench(boardPos5, 1), 44ULL);
    test("perft(5) position 2 kiwipete", perft::perftBench(boardPos2, 5), 193690690ULL);

    std::cout << std::endl;
    std::cout << "Tests passed: " << passed << std::endl;
    std::cout << "Tests failed: " << failed << std::endl;
    
    return 0;
}

