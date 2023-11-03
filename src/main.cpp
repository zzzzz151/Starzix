// clang-format off

#include <stdio.h>
#include <array>
#include <cstring> // for memset()
#include <chrono>
#include "board.hpp"
#include "search.hpp"
#include "uci.hpp"

using namespace std;

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();
    initSearch();
    board = Board(START_FEN);
    uci::uciLoop();
    return 0;
}


