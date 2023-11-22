// clang-format off

#include "board.hpp"

Board board;

#include "search.hpp"
#include "uci.hpp"

int main()
{
    Board::initZobrist();
    attacks::init();
    nnue::loadNetFromFile();
    search::init();
    board = Board(START_FEN);
    uci::uciLoop();
    return 0;
}


