// clang-format off

#include "utils.hpp"
#include "searcher.hpp"
#include "uci.hpp"

int main(int argc, char* argv[])
{
    std::cout << "Starzix by zzzzz" << std::endl;

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512 (fastest)" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2 (fast)" << std::endl;
    #else
        std::cout << "Not using avx2 or avx512 (slow)" << std::endl;
    #endif

    Searcher searcher = Searcher();
    printTTSize(searcher.mTT);
    
    // If a command is passed in program args, run it and exit

    std::string command = "";

    for (int i = 1; i < argc; i++)
        command += std::string(argv[i]) + " ";

    trim(command);

    if (command != "") {
        uci::runCommand(command, searcher);
        return 0;
    }

    // UCI loop
    while (uci::runCommand(command, searcher))
        std::getline(std::cin, command);

    return 0;
}
