// clang-format off

#include "../src/position.hpp"
#include "../src/move_gen.hpp"
#include "positions.hpp"
#include <cassert>

int main() {
    std::cout << colored("Running game state tests...", ColorCode::Yellow) << std::endl;

    // Draw by 50 moves
    Position pos = POS_KIWIPETE;
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.setPliesSincePawnOrCapture(100);
    assert(pos.gameState(hasLegalMove) == GameState::Draw);

    // In check (but has legal move) is ongoing unless draw by 50 move counter
    pos = POS_IN_CHECK;
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.setPliesSincePawnOrCapture(100);
    assert(pos.gameState(hasLegalMove) == GameState::Draw);

    // Checkmate
    pos = POS_CHECKMATE;
    assert(pos.gameState(hasLegalMove) == GameState::Loss);
    pos.setPliesSincePawnOrCapture(100);
    assert(pos.gameState(hasLegalMove) == GameState::Loss);

    // Stalemate
    pos = POS_STALEMATE;
    assert(pos.gameState(hasLegalMove) == GameState::Draw);
    pos.setPliesSincePawnOrCapture(100);
    assert(pos.gameState(hasLegalMove) == GameState::Draw);

    // Draw by insuf material
    for (const std::string fen : std::array {
        "k7/8/8/8/8/8/8/K7 w - - 0 1",   // K vs k
        "k7/8/8/8/8/8/8/KN6 w - - 0 1",  // KN vs k
        "kn6/8/8/8/8/8/8/K7 w - - 0 1",  // K vs kn
        "k7/8/8/8/8/8/8/KB6 w - - 0 1",  // KB vs k
        "kb6/8/8/8/8/8/8/K7 w - - 0 1",  // K vs kb
        "k7/8/8/8/8/8/8/KNN5 w - - 0 1", // KNN vs k
        "knn5/8/8/8/8/8/8/K7 w - - 0 1", // K vs knn
        "kn6/8/8/8/8/8/8/KN6 w - - 0 1", // KN vs kn
        "kb6/8/8/8/8/8/8/KN6 w - - 0 1", // KN vs kb
        "kn6/8/8/8/8/8/8/KB6 w - - 0 1", // KB vs kn
        "kb6/8/8/8/8/8/8/KB6 w - - 0 1", // KB vs kb
    }) {
        assert(Position(fen).gameState(hasLegalMove) == GameState::Draw);
    }

    // Almost insuf material is not draw
    for (const std::string fen : std::array {
        "k7/8/8/8/8/8/8/KNB5 w - - 0 1", // KNB vs k
        "knb5/8/8/8/8/8/8/K7 w - - 0 1", // K vs knb
        "k7/8/8/8/8/8/8/KBB5 w - - 0 1", // KBB vs k
        "kbb5/8/8/8/8/8/8/K7 w - - 0 1", // K vs kbb
    }) {
        assert(Position(fen).gameState(hasLegalMove) == GameState::Ongoing);
    }

    // Draw by repetition
    pos = START_POS;
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.makeMove("g1f3");
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.makeMove("g8f6");
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.makeMove("f3g1");
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.makeMove("f6g8");
    assert(pos.gameState(hasLegalMove) == GameState::Draw);
    pos.makeMove("b1c3");
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);
    pos.undoMove();
    assert(pos.gameState(hasLegalMove) == GameState::Draw);
    pos.undoMove();
    assert(pos.gameState(hasLegalMove) == GameState::Ongoing);

    std::cout << colored("Game state tests passed", ColorCode::Green) << std::endl;
}
