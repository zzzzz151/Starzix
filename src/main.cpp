// clang-format off

#include <cstring> // for memset()
#include <chrono>
#include <array>

#include "board/board.hpp"
#include "search.hpp"
#include "uci.hpp"

using namespace std;

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();
    initTT();
    initLmrTable();
    uci::uciLoop();
    return 0;
}


