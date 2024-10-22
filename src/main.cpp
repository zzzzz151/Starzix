// clang-format off

#include "utils.hpp"
#include "board.hpp"
#include "search.hpp"
#include "uci.hpp"
#include "searcher.hpp"

// On Linux, include the library needed to set stack size
#if defined(__GNUC__)
    #include <sys/resource.h>
#endif

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

    // On Windows, stack size is increased through a compiler flag
    // On Linux, we have to increase it with setrlimit()
    #if defined(__GNUC__)

        const rlim_t stackSize = 16 * 1024 * 1024; // 16 MB
        struct rlimit rl;
        int result = getrlimit(RLIMIT_STACK, &rl);

        if (result == 0 && rl.rlim_cur < stackSize) 
        {
            rl.rlim_cur = stackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            //std::cout << "setrlimit result " << result << std::endl;
            if (result != 0) exit(EXIT_FAILURE);
        }

    #endif

    Searcher searcher = Searcher();
    
    // If a command is passed in program args, run it and exit

    std::string command = "";

    for (int i = 1; i < argc; i++)
        command += std::string(argv[i]) + " ";

    trim(command);

    if (command != "") {
        uci::runCommand(command, searchThread);
        return 0;
    }

    // UCI loop
    while (true) {
        std::getline(std::cin, command);
        uci::runCommand(command, searcher);
    }

    return 0;
}
