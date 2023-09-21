#include <iostream>
#include "board.hpp"

int main()
{
    cout << "Running board tests..." << endl << endl;

    Board board = Board(START_FEN);
    board.printBoard();
    cout << endl;

    if (board.getFen() != START_FEN)
    {
        cout << "[FAILED]" << endl << "Expected: " << START_FEN << endl << "Got: " << board.getFen() << endl;
    }
    return 0;
}