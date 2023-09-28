#include <iostream>
#include <cstdint>
#include <string>
#include "../src-test/board/board.hpp"
#include "../src-test/nnue.hpp"

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();

    Board board = Board(START_FEN);

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    board.makeMove("e2e4");

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    board.makeMove("a7a5");

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    board.undoMove();

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << "Rebuilding board..." << endl;

    board = Board(START_FEN);

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    board.makeMove("e2e4");

    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    return 0;
}