// clang-format-off

#pragma once

#include <thread>
#include "board.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "bench.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(std::vector<std::string> &tokens, std::vector<TTEntry> &tt);
inline void position(std::vector<std::string> &tokens, Board &board);
inline void go(std::vector<std::string> &tokens, Board &board);

inline void runCommand(std::string &command, Board &board, std::vector<TTEntry> &tt)
{
    trim(command);
    std::vector<std::string> tokens = splitString(command, ' ');

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
    else if (tokens[0] == "print" || tokens[0] == "d"
    || tokens[0] == "display" || tokens[0] == "show")
        board.print();
    else if (tokens[0] == "eval") 
    {
        Accumulator acc = Accumulator(board);

        std::cout << "eval "    << evaluate(&acc, board, false)
                  << " scaled " << evaluate(&acc, board, true)
                  << std::endl;
    }
    else if (tokens[0] == "bench")
    {
        if (tokens.size() == 1)
            bench();
        else {
            int depth = stoi(tokens[1]);
            bench(depth);
        }
    }
    else if (tokens[0] == "perft" || (tokens[0] == "go" && tokens[1] == "perft"))
    {
        int depth = stoi(tokens.back());
        perftBench(board, depth);
    }
    else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
    || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
    {
        int depth = stoi(tokens[1]);
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
    else if (command == "paramsjson")
        printParamsAsJson();
}

inline void uci() {
    std::cout << "id name Starzix" << std::endl;
    std::cout << "id author zzzzz" << std::endl;
    std::cout << "option name Hash type spin default 32 min 1 max 65536" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 256" << std::endl;

    /*
    for (auto [paramName, tunableParam] : tunableParams) {
        std::cout << "option name " << paramName;

        std::visit([&] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            if (std::is_same<decltype(myParam->value), double>::value
            || std::is_same<decltype(myParam->value), float>::value)
            {
                std::cout << " type spin default " << round(myParam->value * 100.0)
                          << " min " << round(myParam->min * 100.0)
                          << " max " << round(myParam->max * 100.0);
            }
            else {
                std::cout << " type spin default " << myParam->value
                          << " min " << myParam->min
                          << " max " << myParam->max;
            }
        }, tunableParam);

        std::cout << std::endl;
    }
    */

    std::cout << "uciok" << std::endl;
}

inline void setoption(std::vector<std::string> &tokens, std::vector<TTEntry> &tt)
{
    std::string optionName = tokens[2];
    std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        resizeTT(tt, stoll(optionValue));
        printTTSize(tt);
    }
    else if (optionName == "Threads" || optionName == "threads")
    {
        int numThreads = std::clamp(stoi(optionValue), 1, 256);

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
        i64 newValue = stoll(optionValue);

        std::visit([optionName, newValue] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            myParam->value = std::is_same<decltype(myParam->value), double>::value 
                             || std::is_same<decltype(myParam->value), float>::value 
                             ? (double)newValue / 100.0 
                             : newValue;

            if (optionName == stringify(lmrBase) || optionName == stringify(lmrMultiplier))
                initLmrTable();
            else if (optionName == stringify(seePawnValue)
            || optionName == stringify(seeMinorValue)
            || optionName == stringify(seeRookValue)
            || optionName == stringify(seeQueenValue))
                SEE_PIECE_VALUES = {
                    seePawnValue(),  // Pawn
                    seeMinorValue(), // Knight
                    seeMinorValue(), // Bishop
                    seeRookValue(),  // Rook
                    seeQueenValue(), // Queen
                    0,               // King
                    0                // None
                };

            std::cout << optionName << " set to " << myParam->value << std::endl;
        }, tunableParam);
    }
}

inline void position(std::vector<std::string> &tokens, Board &board)
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

inline void go(std::vector<std::string> &tokens, Board &board)
{
    std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    i32 maxDepth = MAX_DEPTH;
    i64 milliseconds = I64_MAX;
    i64 incrementMs = 0;
    i64 movesToGo = defaultMovesToGo();
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

    i64 maxHardMilliseconds = std::max((i64)0, milliseconds - 10);
    u64 hardMilliseconds, softMilliseconds;

    if (isMoveTime || maxHardMilliseconds <= 0) {
        hardMilliseconds = maxHardMilliseconds;
        softMilliseconds = I64_MAX;
    }
    else {
        hardMilliseconds = round(maxHardMilliseconds * hardTimePercentage());
        double softMs = (double)maxHardMilliseconds / (double)movesToGo + (double)incrementMs * 0.6666;
        softMilliseconds = round(softMs * softTimePercentage());
        softMilliseconds = std::min(softMilliseconds, hardMilliseconds);
    }

    SearchThread::sSearchStopped = false;
    std::vector<std::thread> threads;

    // Start secondary threads search
    for (u64 i = 1; i < gSearchThreads.size(); i++)
        threads.emplace_back([&, i]() {
            gSearchThreads[i].search(
                board, maxDepth, startTime, softMilliseconds, hardMilliseconds, I64_MAX, maxNodes);
        });

    // Main thread search
    gMainThread->search(board, maxDepth, startTime, softMilliseconds, hardMilliseconds, I64_MAX, maxNodes);

    // Wait for secondary threads
    for (auto &thread : threads)
        thread.join();

    std::cout << "bestmove " << gMainThread->bestMoveRoot().toUci() << std::endl;
}

} // namespace uci


