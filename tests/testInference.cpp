#include <iostream>
#include <cstdint>
#include <string>
#include "../src-test/board.hpp"
#include "../src-test/nnue.hpp"

int main()
{
    Board::initZobrist();
    attacks::init();
    nnue::loadNetFromFile();

    Board board = Board(START_FEN);
    
    std::cout << "Start pos" << std::endl;
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "e2e4" << std::endl;
    board.makeMove("e2e4");
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "a7a5" << std::endl;
    board.makeMove("a7a5");
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "undo a7a5" << std::endl;
    board.undoMove();
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "Rebuilding start pos..." << std::endl;

    board = Board(START_FEN);

    std::cout << std::endl << "Start pos" << std::endl;
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "e2e4" << std::endl;
    board.makeMove("e2e4");
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    assert(!board.inCheck());

    std::cout << std::endl << "null move" << std::endl;
    board.makeNullMove();
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "undo null move" << std::endl;
    board.undoNullMove();
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "Custom sicilian position" << std::endl;
    board = Board("rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    std::cout << std::endl << "d7d6" << std::endl;
    board.makeMove("d7d6");
    std::cout << "Color::WHITE eval " << nnue::evaluate(Color::WHITE) << std::endl;
    std::cout << "Color::BLACK eval " << nnue::evaluate(Color::BLACK) << std::endl;

    return 0;
}