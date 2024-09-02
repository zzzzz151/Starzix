// clang-format-off

#pragma once

#include "board.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "bench.hpp"
#include <thread>

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(const std::vector<std::string> &tokens, std::vector<TTEntry> &tt);
inline void position(const std::vector<std::string> &tokens, Board &board);
inline void go(const std::vector<std::string> &tokens, const Board &board);

inline void runCommand(std::string &command, Board &board, std::vector<TTEntry> &tt)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    if (!std::cin.good())
        exit(EXIT_FAILURE);
    else if (command == "" || tokens.size() == 0)
        return;
    else if (command == "quit")
        exit(EXIT_SUCCESS);
    else if (command == "uci")
        uci();
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
        setoption(tokens, tt);
    else if (command == "ucinewgame")
    {
        board = Board(START_FEN);
        
        resetTT(tt);

        for (auto &searchThread : gSearchThreads) 
            searchThread.reset();
    }
    else if (command == "isready")
        std::cout << "readyok" << std::endl;
    else if (tokens[0] == "position")
        position(tokens, board);
    else if (tokens[0] == "go")
        go(tokens, board);
    else if (command == "print" || command == "d"
    || command == "display" || command == "show")
        board.print();
    else if (command == "eval") 
    {
        const BothAccumulators acc = BothAccumulators(board);
        const i32 eval = nnue::evaluate(&acc, board.sideToMove());
        const i32 evalScaled = eval * materialScale(board);

        std::cout << "eval "    << eval
                  << " scaled " << evalScaled
                  << std::endl;
    }
    else if (tokens[0] == "bench")
    {
        if (tokens.size() == 1)
            bench();
        else {
            const int depth = stoi(tokens[1]);
            bench(depth);
        }
    }
    else if (tokens[0] == "perft" || (tokens[0] == "go" && tokens[1] == "perft"))
    {
        const int depth = stoi(tokens.back());
        perftBench(board, depth);
    }
    else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
    || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
    {
        const int depth = stoi(tokens[1]);
        perftSplit(board, depth);
    }
    else if (tokens[0] == "makemove")
    {
        if ((tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
        && !board.inCheck())
            board.makeMove(MOVE_NONE);
        else
            board.makeMove(tokens[1]);
    }
    else if (tokens[0] == "undomove")
        board.undoMove();
    else if (command == "spsainput") 
    {
        for (auto &pair : tunableParams) {
            std::string paramName = pair.first;
            auto &tunableParam = pair.second;

            std::visit([&paramName] (auto *myParam) 
            {
                if (myParam == nullptr) return;

                std::cout << paramName 
                          << ", " << (myParam->floatOrDouble() ? "float" : "int")
                          << ", " << myParam->value
                          << ", " << myParam->min
                          << ", " << myParam->max
                          << ", " << myParam->step
                          << ", 0.002"
                          << std::endl;
            }, tunableParam);
        }
    }

}

inline void uci() {
    std::cout << "id name Starzix" << std::endl;
    std::cout << "id author zzzzz" << std::endl;
    std::cout << "option name Hash type spin default 32 min 1 max 65536" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 256" << std::endl;

    /*
    for (auto &pair : tunableParams) {
        std::string paramName = pair.first;
        auto &tunableParam = pair.second;

        std::visit([&paramName] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            std::cout << "option name " << paramName 
                      << " type string"
                      << " default " << myParam->value
                      << " min "     << myParam->min
                      << " max "     << myParam->max
                      << std::endl;
            
        }, tunableParam);
    }
    */

    std::cout << "uciok" << std::endl;
}

inline void setoption(const std::vector<std::string> &tokens, std::vector<TTEntry> &tt)
{
    const std::string optionName  = tokens[2];
    const std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        resizeTT(tt, stoll(optionValue));
        printTTSize(tt);
    }
    else if (optionName == "Threads" || optionName == "threads")
    {
        const int numThreads = std::clamp(stoi(optionValue), 1, 256);

        // Remove all threads except main thread
        while (gSearchThreads.size() > 1)
            gSearchThreads.pop_back();

        while ((int)gSearchThreads.size() < numThreads)
            gSearchThreads.push_back(SearchThread(&tt));

        gSearchThreads.shrink_to_fit();

        gMainThread = &gSearchThreads[0]; // push_back() or shrink_to_fit() may reallocate

        std::cout << "Threads set to " << numThreads << std::endl;
    }
    else if (tunableParams.count(optionName) > 0) 
    {
        auto tunableParam = tunableParams[optionName];

        std::visit([optionName, optionValue] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            myParam->value = std::stod(optionValue);

            if (optionName == stringify(lmrBaseQuiet) || optionName == stringify(lmrMultiplierQuiet)
            || optionName == stringify(lmrBaseNoisy) || optionName == stringify(lmrMultiplierNoisy))
                initLmrTable();

            std::cout << optionName << " set to " << myParam->value << std::endl;
        }, tunableParam);
    }
}

inline void position(const std::vector<std::string> &tokens, Board &board)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos") {
        board = Board(START_FEN);
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        u64 i = 0;
        
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";

        fen.pop_back(); // remove last whitespace
        board = Board(fen);
        movesTokenIndex = i;
    }

    for (u64 i = movesTokenIndex + 1; i < tokens.size(); i++)
        board.makeMove(tokens[i]);
}

inline void go(const std::vector<std::string> &tokens, const Board &board)
{
    const std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    i32 maxDepth = MAX_DEPTH;
    i64 milliseconds = I64_MAX;
    i64 incrementMs = 0;
    i64 movesToGo = 0;
    bool isMoveTime = false;
    i64 maxNodes = I64_MAX;

    for (int i = 1; i < (int)tokens.size() - 1; i += 2)
    {
        i64 value = std::stoll(tokens[i + 1]);

        if ((tokens[i] == "wtime" && board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && board.sideToMove() == Color::BLACK))
            milliseconds = std::max(value, (i64)0);

        else if ((tokens[i] == "winc" && board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && board.sideToMove() == Color::BLACK))
            incrementMs = std::max(value, (i64)0);

        else if (tokens[i] == "movestogo")
            movesToGo = std::max(value, (i64)1);
        else if (tokens[i] == "movetime")
        {
            milliseconds = std::max(value, (i64)0);
            isMoveTime = true;
        }
        else if (tokens[i] == "depth")
            maxDepth = value;
        else if (tokens[i] == "nodes")
            maxNodes = value;
    }

    // Calculate search time limits

    u64 hardMilliseconds = std::max((i64)0, milliseconds - 10);
    u64 softMilliseconds = I64_MAX;

    if (!isMoveTime) {
        hardMilliseconds *= hardTimePercentage();
        softMilliseconds = hardMilliseconds * softTimePercentage();
    }

    SearchThread::sSearchStopped = false;
    std::vector<std::thread> threads;

    // Start secondary threads search
    for (u64 i = 1; i < gSearchThreads.size(); i++)
        threads.emplace_back([&, i]() {
            gSearchThreads[i].search(
                board, maxDepth, startTime, hardMilliseconds, softMilliseconds, maxNodes);
        });

    // Main thread search
    gMainThread->search(board, maxDepth, startTime, hardMilliseconds, softMilliseconds, maxNodes);

    // Wait for secondary threads
    for (auto &thread : threads)
        if (thread.joinable())
            thread.join();

    std::cout << "bestmove " << gMainThread->bestMoveRoot().toUci() << std::endl;
}

} // namespace uci
