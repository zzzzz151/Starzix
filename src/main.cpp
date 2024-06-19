// clang-format off

#include "board.hpp"
#include "search.hpp"
#include "uci.hpp"

int main(int argc, char* argv[])
{
    std::cout << "Starzix by zzzzz" << std::endl;

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2" << std::endl;
    #else
        std::cout << "Not using avx2 or avx512" << std::endl;
    #endif

    initUtils();
    initZobrist();
    cuckoo::init();
    initLmrTable();

    Board board = Board(START_FEN);

    std::vector<TTEntry> tt;
    resizeTT(tt, 32);
    printTTSize(tt);
    
    gSearchThreads = { SearchThread(&tt) };
    gMainThread = &gSearchThreads[0];

    // If a command is passed in program args, run it and exit

    std::string command = "";

    for (int i = 1; i < argc; i++)
        command += (std::string)argv[i] + " ";

    trim(command);

    if (command != "") {
        uci::runCommand(command, board, tt);
        return 0;
    }

    // UCI loop
    while (true) {
        std::getline(std::cin, command);
        uci::runCommand(command, board, tt);
    }

    return 0;
}


