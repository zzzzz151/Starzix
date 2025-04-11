// clang-format off

#include "../src/position.hpp"
#include "../src/move_gen.hpp"
#include "positions.hpp"
#include <cassert>

int main() {
    std::cout << colored("Running move gen tests...", ColorCode::Yellow) << std::endl;

    // isPseudolegalLegal()
    Position pos = Position("8/8/2k1qr2/8/4R3/3QK3/8/8 w - - 0 1");
    assert(isPseudolegalLegal(pos, pos.uciToMove("e4e5")));
    assert(isPseudolegalLegal(pos, pos.uciToMove("e4e6")));
    assert(!isPseudolegalLegal(pos, pos.uciToMove("e4c4")));
    assert(!isPseudolegalLegal(pos, pos.uciToMove("e4f4")));

    // hasLegalMove()
    pos = START_POS;
    assert(hasLegalMove(pos));
    pos = POS_IN_CHECK;
    assert(hasLegalMove(pos));
    pos = POS_CHECKMATE;
    assert(!hasLegalMove(pos));
    pos = POS_STALEMATE;
    assert(!hasLegalMove(pos));

    // Perft

    Position startPos     = START_POS;
    Position posKiwipete  = POS_KIWIPETE;
    Position pos3         = POS_3;
    Position pos4         = POS_4;
    Position pos4Mirrored = POS_4_MIRRORED;
    Position pos5         = POS_5;

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
