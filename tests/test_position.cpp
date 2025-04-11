// clang-format off

#include "../src/position.hpp"
#include "positions.hpp"
#include <cassert>

int main() {
    std::cout << colored("Running position tests...", ColorCode::Yellow) << std::endl;

    // Position.fen()

    assert(START_POS     .fen() == FEN_START);
    assert(POS_KIWIPETE  .fen() == FEN_KIWIPETE + "0 1");
    assert(POS_3         .fen() == FEN_POS_3);
    assert(POS_4         .fen() == FEN_POS_4);
    assert(POS_4_MIRRORED.fen() == FEN_POS_4_MIRRORED);
    assert(POS_5         .fen() == FEN_POS_5);

    // En passant in FEN
    const std::string fen = "rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11";
    assert(Position(fen).fen() == fen);

    // Position.checkers()
    assert(Position("6k1/4r3/8/2n5/4K3/8/8/8 w - - 0 1").checkers() == 4503616807239680ULL);

    // Position.inCheck()

    assert(!Position("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9")
        .inCheck());

    assert(Position("rnbqkbnr/pppp2pp/5pQ1/4p3/3P4/8/PPP1PPPP/RNB1KBNR b KQkq - 1 3")
        .inCheck());

    // Position.pinned()
    assert(Position("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P4/1P2PP2/P1P3PP/RNBQK1NR b KQkq - 0 5")
        .pinned() == squareBb(Square::C6));

    // Position.attacks()
    Position pos = Position("5k2/2p5/2r5/8/1N6/3K4/8/8 w - - 0 1");
    assert(pos.attacks(Color::White) == 5532389481728ULL);
    assert(pos.attacks(Color::Black) == 5797534614998483972ULL);

    // Position.attackers()
    assert(Position("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P2P1/1P2PP2/P1P4P/RNBQK1NR b KQkq - 0 5")
        .attackers(Square::F5) == 288230652103360512ULL);

    // Position.captured(Move) and Position.isQuiet(Move)

    const Move quiet = Move(Square::E2, Square::D3, MoveFlag::Bishop);
    assert(!POS_KIWIPETE.captured(quiet));
    assert(POS_KIWIPETE.isQuiet(quiet));

    const Move capture = Move(Square::E2, Square::A6, MoveFlag::Bishop);
    assert(POS_KIWIPETE.captured(capture) == PieceType::Bishop);
    assert(!POS_KIWIPETE.isQuiet(capture));

    const Move castle = Move(Square::E1, Square::G1, MoveFlag::Castling);
    assert(POS_KIWIPETE.isQuiet(castle));

    const Move knightPromo = Move(Square::A7, Square::A8, MoveFlag::KnightPromo);
    assert(!Position("7k/P7/8/8/8/8/8/K7 w - - 0 1").isQuiet(knightPromo));

    pos = Position("rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    const Move enPassant = Move(Square::H5, Square::G6, MoveFlag::EnPassant);
    assert(pos.captured(enPassant) == PieceType::Pawn);
    assert(!pos.isQuiet(enPassant));

    // Position.uciToMove(std::string)

    const Move d2d4 = Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush);
    assert(START_POS.uciToMove("d2d4") == d2d4);

    assert(POS_KIWIPETE.uciToMove("e2a6") == capture);
    assert(POS_KIWIPETE.uciToMove("e1g1") == castle);

    pos = Position("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    assert(pos.uciToMove("a7a8n") == knightPromo);

    const Move queenPromo = Move(Square::A7, Square::A8, MoveFlag::QueenPromo);
    assert(pos.uciToMove("a7a8q") == queenPromo);
    assert(pos.uciToMove("a7a8") == queenPromo);

    assert(Position("rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11")
        .uciToMove("h5g6") == enPassant);

    // Position.makeMove(Move)

    pos = Position("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9");

    // black promotes to queen
    pos.makeMove("a2b1q");
    assert(pos.fen() == "rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQK2R w KQkq - 0 10");

    // white castles short
    pos.makeMove("e1g1");
    assert(pos.fen() == "rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 1 10");

    // black allows en passant
    pos.makeMove("g7g5");
    assert(pos.fen() == "rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");

    // white captures en passant
    pos.makeMove("h5g6");
    assert(pos.fen() == "rnbqkb1r/4pp1p/1p1p1nP1/2p5/2BP2P1/4PN2/2P2P2/RqBQ1RK1 b kq - 0 11");

    // black captures white pawn with black knight
    pos.makeMove("f6g4");
    assert(pos.fen() == "rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/2P2P2/RqBQ1RK1 w kq - 0 12");

    // white moves rook up (test 50move counter)
    pos.makeMove("a1a2");
    assert(pos.fen() == "rnbqkb1r/4pp1p/1p1p2P1/2p5/2BP2n1/4PN2/R1P2P2/1qBQ1RK1 b kq - 1 12");

    // Position.zobristHash()
    assert(pos.zobristHash() == Position(pos.fen()).zobristHash());

    // Make null move
    pos = Position("1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 w Qk f6 0 15");
    pos.makeMove(MOVE_NONE);
    assert(pos.fen() == "1rq1kbnr/p2b2p1/1p2p2p/3p1pP1/1Q1pP3/1PP4P/P2B1P1R/RN2KBN1 b Qk - 1 15");

    std::cout << colored("Position tests passed", ColorCode::Green) << std::endl;
}
