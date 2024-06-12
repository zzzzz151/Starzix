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
    initUtils();
    cuckoo::init();
    initLmrTable();

    std::vector<TTEntry> tt;
    resizeTT(tt, 32);
    printTTSize(tt);
    
    searchThreads = { SearchThread(&tt) };
    mainThread = &searchThreads[0];

    uci::uciLoop(tt);
    return 0;
}


