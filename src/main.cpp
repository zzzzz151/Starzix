// clang-format off

#include "uci.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "Starzix by zzzzz" << std::endl;

    #if defined(__AVX512F__) && defined(__AVX512BW__)
        std::cout << "Using avx512 (fastest)" << std::endl;
    #elif defined(__AVX2__)
        std::cout << "Using avx2 (fast)" << std::endl;
    #else
        std::cout << "Not using avx2 nor avx512 (slow)" << std::endl;
    #endif

    Position pos = START_POS;
    Searcher searcher = Searcher();
    printTTSize(searcher.mTT);

    // If a command is passed in program args, run it and exit

    std::string command = "";

    for (size_t i = 1; i < static_cast<size_t>(argc); i++)
        command += " " + std::string(argv[i]);

    trim(command);

    if (command != "")
    {
        try {
            uci::runCommand(command, pos, searcher);
        }
        catch (...) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    // UCI loop
    while (true)
    {
        std::getline(std::cin, command);
        uci::runCommand(command, pos, searcher);
    }

    return EXIT_SUCCESS;
}
