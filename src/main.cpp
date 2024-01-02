// clang-format off

#include "board.hpp"
#include "search.hpp"
#include "uci.hpp"

int main()
{
    std::cout << "z5 by zzzzz" << std::endl;
    attacks::init();
    initLmrTable();
    Searcher searcher = Searcher(Board(START_FEN, true));
    uci::uciLoop(searcher);
    return 0;
}


