// clang-format off

#include "../src/position.hpp"
#include <cassert>

const std::string FEN_KIWIPETE       = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const std::string FEN_KIWIPETE_FIXED = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
const std::string FEN_POS_3          = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
const std::string FEN_POS_4          = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const std::string FEN_POS_4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
const std::string FEN_POS_5          = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";

int main() {
    std::cout << colored("Running position tests...", ColorCode::Yellow) << std::endl;

    // Position.fen()

    assert(START_POS                   .fen() == START_FEN);
    assert(Position(FEN_KIWIPETE)      .fen() == FEN_KIWIPETE_FIXED);
    assert(Position(FEN_POS_3)         .fen() == FEN_POS_3);
    assert(Position(FEN_POS_4)         .fen() == FEN_POS_4);
    assert(Position(FEN_POS_4_MIRRORED).fen() == FEN_POS_4_MIRRORED);
    assert(Position(FEN_POS_5)         .fen() == FEN_POS_5);

    const std::string fen = "rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11";
    assert(Position(fen).fen() == fen);

    // Position.checkers()
    Position pos = Position("6k1/4r3/8/2n5/4K3/8/8/8 w - - 0 1");
    assert(pos.checkers() == 4503616807239680ULL);
    //pos.makeMove("e4f4");
    //assert(pos.checkers() == 0);

    // Position.inCheck()
    assert(!Position("rnbqkb1r/4pppp/1p1p1n2/2p4P/2BP2P1/4PN2/p1P2P2/RNBQK2R b KQkq - 5 9").inCheck());
    assert(Position("rnbqkbnr/pppp2pp/5pQ1/4p3/3P4/8/PPP1PPPP/RNB1KBNR b KQkq - 1 3").inCheck());

    // Position.pinned()
    pos = Position("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P4/1P2PP2/P1P3PP/RNBQK1NR b KQkq - 0 5");
    assert(pos.pinned() == squareBb(Square::C6));

    // Position.attacks()
    pos = Position("5k2/2p5/2r5/8/1N6/3K4/8/8 w - - 0 1");
    assert(pos.attacks(Color::White) == 5532389481728ULL);
    assert(pos.attacks(Color::Black) == 5797534614998483972ULL);

    // Position.attackers()
    pos = Position("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P2P1/1P2PP2/P1P4P/RNBQK1NR b KQkq - 0 5");
    assert(pos.attackers(Square::F5) == 288230652103360512ULL);

    // Position.captured(Move) and Position.isQuiet(Move)

    pos = Position(FEN_KIWIPETE);

    const Move quiet = Move(Square::E2, Square::D3, MoveFlag::Bishop);
    assert(!pos.captured(quiet));
    assert(pos.isQuiet(quiet));

    const Move capture = Move(Square::E2, Square::A6, MoveFlag::Bishop);
    assert(pos.captured(capture) == PieceType::Bishop);
    assert(!pos.isQuiet(capture));

    const Move castle = Move(Square::E1, Square::G1, MoveFlag::Castling);
    assert(pos.isQuiet(castle));

    pos = Position("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    const Move knightPromo = Move(Square::A7, Square::A8, MoveFlag::KnightPromo);
    assert(!pos.isQuiet(knightPromo));

    pos = Position("rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    const Move enPassant = Move(Square::H5, Square::G6, MoveFlag::EnPassant);
    assert(pos.captured(enPassant) == PieceType::Pawn);
    assert(!pos.isQuiet(enPassant));

    // Position.uciToMove(std::string)

    const Move pawnDoublePush = Move(Square::D2, Square::D4, MoveFlag::PawnDoublePush);
    assert(START_POS.uciToMove("d2d4") == pawnDoublePush);

    pos = Position(FEN_KIWIPETE);
    assert(pos.uciToMove("e2a6") == capture);
    assert(pos.uciToMove("e1g1") == castle);

    pos = Position("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    assert(pos.uciToMove("a7a8n") == knightPromo);

    const Move queenPromo = Move(Square::A7, Square::A8, MoveFlag::QueenPromo);
    assert(pos.uciToMove("a7a8q") == queenPromo);
    assert(pos.uciToMove("a7a8") == queenPromo);

    pos = Position("rnbqkb1r/4pp1p/1p1p1n2/2p3pP/2BP2P1/4PN2/2P2P2/RqBQ1RK1 w kq g6 0 11");
    assert(pos.uciToMove("h5g6") == enPassant);

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

    // Position.roughHashAfter()
    pos = Position("r1b1kbnr/ppp2ppp/2np4/1B2p1q1/3P4/1P2PP2/P1P3PP/RNBQK1NR b KQkq - 0 5");
    const Move move = Move(Square::G5, Square::E3, MoveFlag::Queen);
    const auto hashAfter = pos.roughHashAfter(move);
    pos.makeMove(move);
    assert(hashAfter == pos.zobristHash());

    // Position.isDrawIgnoringStalemate()

    // Draw by plies since pawn move or capture >= 100
    assert(!Position(FEN_KIWIPETE + "99 0").isDrawIgnoringStalemate());
    assert(Position(FEN_KIWIPETE + "100 0").isDrawIgnoringStalemate());
    assert(Position(FEN_KIWIPETE + "101 0").isDrawIgnoringStalemate());

    // Draw by insuf material
    assert(Position("k7/8/8/8/8/8/8/K7 w - - 0 1").isDrawIgnoringStalemate());
    assert(Position("k7/8/8/7b/8/8/8/K7 w - - 0 1").isDrawIgnoringStalemate());
    assert(Position("k7/8/8/8/8/8/8/KN6 w - - 0 1").isDrawIgnoringStalemate());
    assert(!Position("k7/8/8/8/8/8/8/KNB5 w - - 0 1").isDrawIgnoringStalemate());
    assert(!Position("kn6/8/8/8/8/8/8/KB6 w - - 0 1").isDrawIgnoringStalemate());

    // Draw by repetition
    pos = START_POS;
    assert(!pos.isDrawIgnoringStalemate());
    pos.makeMove("g1f3");
    assert(!pos.isDrawIgnoringStalemate());
    pos.makeMove("g8f6");
    assert(!pos.isDrawIgnoringStalemate());
    pos.makeMove("f3g1");
    assert(!pos.isDrawIgnoringStalemate());
    pos.makeMove("f6g8");
    assert(pos.isDrawIgnoringStalemate());
    pos.makeMove("b1c3");
    assert(!pos.isDrawIgnoringStalemate());
    pos.undoMove();
    assert(pos.isDrawIgnoringStalemate());
    pos.undoMove();
    assert(!pos.isDrawIgnoringStalemate());

    std::cout << colored("Position tests passed", ColorCode::Green) << std::endl;
}
