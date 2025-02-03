// clang-format off

#include "../src/position.hpp"
#include "../src/move_gen.hpp"
#include <cassert>

int main() {
    std::cout << colored("Running move gen tests...", ColorCode::Yellow) << std::endl;

    // isPseudolegalLegal()
    Position pos = Position("8/8/2k1qr2/8/4R3/3QK3/8/8 w - - 0 1");
    assert(isPseudolegalLegal(pos, pos.uciToMove("e4e5")));
    assert(isPseudolegalLegal(pos, pos.uciToMove("e4e6")));
    assert(!isPseudolegalLegal(pos, pos.uciToMove("e4c4")));
    assert(!isPseudolegalLegal(pos, pos.uciToMove("e4f4")));

    // Perft

    Position startPos     = START_POS;
    Position posKiwipete  = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    Position pos3         = Position("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    Position pos4         = Position("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    Position pos4Mirrored = Position("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
    Position pos5         = Position("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");

    assert(perft(startPos, 0)  == 1ULL); // perft(0)
    assert(perft(startPos, -1) == 1ULL); // perft(-1)

    // perft(1)
    assert(perft(startPos, 1) == 20ULL);
    assert(perft(posKiwipete, 1) == 48ULL);
    assert(perft(pos3, 1) == 14ULL);
    assert(perft(pos4, 1) == 6ULL);
    assert(perft(pos4Mirrored, 1) == 6ULL);
    assert(perft(pos5, 1) == 44ULL);

    // Start pos perft's
    assert(perft(startPos, 2) == 400ULL);
    assert(perft(startPos, 3) == 8902ULL);
    assert(perft(startPos, 4) == 197281ULL);
    assert(perft(startPos, 5) == 4865609ULL);
    assert(perft(startPos, 6) == 119060324ULL);

    // Position Kiwipete perft(5)
    assert(perft(posKiwipete, 5) == 193690690ULL);

    std::cout << colored("Move gen tests passed", ColorCode::Green) << std::endl;
}
