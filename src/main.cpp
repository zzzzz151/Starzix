// clang-format off

#include "board.hpp"
#include "search.hpp"
#include "uci.hpp"

int main()
{
    std::cout << "Starzix by zzzzz" << std::endl;

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2" << std::endl;
    #else
        std::cout << "Not using avx2 or avx512" << std::endl;
    #endif

    initZobrist();
    attacks::init();
    initLmrTable();
    Searcher searcher = Searcher(START_BOARD);
    uci::uciLoop(searcher);
    return 0;
}


