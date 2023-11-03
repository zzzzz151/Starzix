#include <iostream>
#include <cstdint>
#include <string>
#include "../src-test/board.hpp"
#include "../src-test/nnue.hpp"

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();

    Board board = Board(START_FEN);
    
    cout << "Start pos" << endl;
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "e2e4" << endl;
    board.makeMove("e2e4");
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "a7a5" << endl;
    board.makeMove("a7a5");
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "undo a7a5" << endl;
    board.undoMove();
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "Rebuilding start pos..." << endl;

    board = Board(START_FEN);

    cout << endl << "Start pos" << endl;
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "e2e4" << endl;
    board.makeMove("e2e4");
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "null move" << endl;
    board.makeNullMove();
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "undo null move" << endl;
    board.undoNullMove();
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "Custom sicilian position" << endl;
    board = Board("rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    cout << endl << "d7d6" << endl;
    board.makeMove("d7d6");
    cout << "white eval " << nnue::evaluate(WHITE) << endl;
    cout << "black eval " << nnue::evaluate(BLACK) << endl;

    return 0;
}