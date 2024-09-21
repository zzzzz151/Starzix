// clang-format off
#include "../src/board.hpp"
#include "../src/perft.hpp"

const std::string POSITION2_KIWIPETE = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const std::string POSITION3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
const std::string POSITION4 = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const std::string POSITION4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ";
const std::string POSITION5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ";

int main()
{   
    initUtils();
    initZobrist();

    // Test utils.hpp

    assert(bitboard(lsb(12)) == 4);

    assert(strToSquare("b7") == 49);

    assert(squareFile(9) == File::B);
    assert(squareRank(41) == Rank::RANK_6);

    assert(BETWEEN[32][59] == (bitboard(41) | bitboard(50)));
    assert(LINE_THROUGH[41][50] == (bitboard(32) | bitboard(41) | bitboard(50) | bitboard(59)));

    // Move tests
    
    Move move = Move(49, 55, Move::NULL_FLAG);
    assert(move.from() == 49);
    assert(move.to() == 55);
    assert(move.flag() == Move::NULL_FLAG);

    move = Move("e1", "g1", Move::CASTLING_FLAG);
    assert(SQUARE_TO_STR[move.from()] == "e1");
    assert(SQUARE_TO_STR[move.to()] == "g1");
    assert(move.flag() == Move::CASTLING_FLAG);
    assert(move.pieceType() == PieceType::KING);

    move = Move("b7", "c8", Move::BISHOP_PROMOTION_FLAG);
    assert(move.toUci() == "b7c8b");
    assert(move.pieceType() == PieceType::PAWN);
    assert(move.promotion() == PieceType::BISHOP);

    // Board tests
    
    // fen()
    Board board = Board(START_FEN);
    Board board2 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    Board board3 = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0");
    assert(board.fen() == START_FEN);
    assert(board2.fen() == "1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    assert(board3.fen() == "1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk f6 0 1");

    // bitboards
    assert(board.oppSide() == Color::BLACK);
    assert(board.occupancy() == 18446462598732906495ULL);
    assert(board.us() == 65535ULL);
    assert(board.them() == 18446462598732840960ULL);
    assert(board.getBb(PieceType::KNIGHT) == 4755801206503243842ULL);
    assert(board.getBb(board.sideToMove(), PieceType::KNIGHT) == 66ULL);
    
    // Pawn double push creates enpassant square
    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove("g7g5");
    assert(board.fen() == "rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    
    // Test position repetition
    board = Board(START_FEN);
    assert(!board.isRepetition());
    board.makeMove("g1f3");
    assert(!board.isRepetition());
    board.makeMove("g8f6");
    assert(!board.isRepetition());
    board.makeMove("f3g1");
    assert(!board.isRepetition());
    board.makeMove("f6g8");
    assert(board.isRepetition());
    board.makeMove("b1c3");
    assert(!board.isRepetition());
    board.undoMove();
    assert(board.isRepetition());
    board.undoMove();
    assert(!board.isRepetition());

    // isSquareAttacked()
    board = Board(POSITION2_KIWIPETE); 
    Color stm = board.sideToMove(), nstm = board.oppSide();
    assert(board.isSquareAttacked(strToSquare("e3"), stm));
    assert(!board.isSquareAttacked(strToSquare("a5"), stm));
    assert(board.isSquareAttacked(strToSquare("d6"), nstm));
    assert(!board.isSquareAttacked(strToSquare("b3"), nstm));
    assert(board.isSquareAttacked(strToSquare("d7"), stm));
    assert(!board.isSquareAttacked(strToSquare("b4"), stm));
    assert(board.isSquareAttacked(strToSquare("e2"), nstm));
    assert(!board.isSquareAttacked(strToSquare("h2"), nstm));

    // attackers()
    board = Board("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P2P1/1P2PP2/P1P4P/RNBQK1NR b KQkq - 0 5");
    assert(board.attackers(strToSquare("f5")) == 288230652103360512ULL);

    // attacks()
    board = Board("5k2/2p5/2r5/8/1N6/3K4/8/8 w - - 0 1");
    assert(board.attacks(Color::WHITE) == 5532389481728ULL);
    assert(board.attacks(Color::BLACK) == 5797534614998483972ULL);

    // Test inCheck()
    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    assert(!board.inCheck());
    board = Board("rnbqkbnr/pppp2pp/5pQ1/4p3/3P4/8/PPP1PPPP/RNB1KBNR b KQkq - 1 3");
    assert(board.inCheck());

    // checkers()
    board = Board("6k1/4r3/8/2n5/4K3/8/8/8 w - - 0 1");
    assert(board.checkers() == 4503616807239680ULL);
    board.makeMove("e4f4");
    assert(board.checkers() == 0);

    // Test null move
    board = Board("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    board.makeMove(MOVE_NONE);
    assert(board.fen() == "1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk - 1 15");
    board.undoMove();
    assert(board.fen() == "1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");

    // Test making moves and zobrist hash

    // makeMove()
    board = Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");
    board.makeMove("a2b1q"); // black promotes to queen
    assert(board.fen() == "rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQK2R w KQkq - 0 10");
    board.makeMove("e1g1");  // white castles short
    assert(board.fen() == "rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");
    board.makeMove("g7g5");  // black allows enpassant
    assert(board.fen() == "rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    board.makeMove("h5g6");  // white takes on passant 
    assert(board.fen() == "rnbqkb1r/4pp1p/1p1p1nP1/2p5/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 0 11");
    board.makeMove("f6g4");  // black captures white pawn with black knight 
    assert(board.fen() == "rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/2P2P2/RqBQ1RK1 w kq - 0 12");
    board.makeMove("a1a2");  // white moves rook up (test 50move counter) 
    assert(board.fen() == "rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");

    u64 hashAfter = board.zobristHash();

    // undoMove() fen
    for (int i = 5; i >= 0; i--)
        board.undoMove();
    assert(board.fen() == "rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");

    // Zobrist hash
    assert(Board("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9").zobristHash() == board.zobristHash());
    assert(Board("rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12").zobristHash() == hashAfter);

    // pinned
    board = Board("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P4/1P2PP2/P1P3PP/RNBQK1NR b KQkq - 0 5");
    assert(board.pinned() == bitboard(42));

    // zobristHashAfter()
    move = Move("g5", "e3", Move::QUEEN_FLAG);
    hashAfter = board.roughHashAfter(move);
    board.makeMove(move);
    assert(hashAfter == board.zobristHash());

    // isPseudolegalLegal()
    board = Board("rnb1kbnr/pppp1ppp/8/4p3/3P4/1P2q3/P1P2PPP/RNBQKBNR w KQkq - 0 4");
    const Move legal = board.uciToMove("f2e3"), illegal = board.uciToMove("g1f3");
    assert(!board.isPseudolegalLegal(illegal));
    assert(board.isPseudolegalLegal(legal));

    // Perft

    board = Board(START_FEN);
    Board boardPos2 = Board(POSITION2_KIWIPETE);
    Board boardPos3 = Board(POSITION3);
    Board boardPos4 = Board(POSITION4);
    Board boardPos4Mirrored = Board(POSITION4_MIRRORED);
    Board boardPos5 = Board(POSITION5);

    // perft(1)
    assert(perft(board, 0) == 1ULL);
    assert(perft(board, 1) == 20ULL);
    assert(perft(boardPos2, 1) == 48ULL);
    assert(perft(boardPos3, 1) == 14ULL);
    assert(perft(boardPos4, 1) == 6ULL);
    assert(perft(boardPos4Mirrored, 1) == 6ULL);
    assert(perft(boardPos5, 1) == 44ULL);

    // start pos perft
    assert(perft(board, 2) == 400ULL);
    assert(perft(board, 3) == 8902ULL);
    assert(perft(board, 4) == 197281ULL);
    assert(perft(board, 5) == 4865609ULL);
    assert(perft(board, 6) == 119060324ULL);

    assert(perft(boardPos2, 5) == 193690690ULL);

    std::cout << "Passed all tests" << std::endl;
    
    return 0;
}

