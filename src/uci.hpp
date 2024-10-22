// clang-format-off

#pragma once

#include "board.hpp"
#include "search.hpp"
#include "bench.hpp"
#include <thread>

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(const std::vector<std::string> &tokens, std::vector<TTEntry>* ttPtr);
inline void position(const std::vector<std::string> &tokens, Board &board);
inline void go(const std::vector<std::string> &tokens, SearchThread &searchThread);

inline void runCommand(std::string &command, SearchThread &searchThread)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    if (!std::cin.good())
        exit(EXIT_FAILURE);
    else if (command == "" || tokens.size() == 0)
        return;
    // UCI commands
    else if (command == "uci")
        uci();
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
        setoption(tokens, searchThread.ttPtr);
    else if (command == "ucinewgame")
        searchThread.reset();
    else if (command == "isready")
        std::cout << "readyok" << std::endl;
    else if (tokens[0] == "position")
        position(tokens, searchThread.mBoard);
    else if (tokens[0] == "go")
        go(tokens, searchThread);
    else if (command == "quit")
        exit(EXIT_SUCCESS);
    // Non-UCI commands
    else if (command == "print" || command == "d"
    || command == "display" || command == "show")
        searchThread.mBoard.print();
    else if (tokens[0] == "bench")
    {
        if (tokens.size() == 1)
            bench();
        else {
            const int depth = stoi(tokens[1]);
            bench(depth);
        }
    }
    else if (command == "eval") 
    {
        const BothAccumulators acc = BothAccumulators(searchThread.mBoard);
        const i32 eval = nnue::evaluate(&acc, searchThread.mBoard.sideToMove());
        const i32 evalScaled = eval * materialScale(searchThread.mBoard);

        std::cout << "eval "    << eval
                  << " scaled " << evalScaled
                  << std::endl;
    }
    else if (tokens[0] == "perft" || (tokens[0] == "go" && tokens[1] == "perft"))
    {
        const int depth = stoi(tokens[1]);
        const std::string fen = searchThread.mBoard.fen();

        std::cout << "perft depth " << depth << " '" << fen << "'" << std::endl;

        const std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
        const u64 nodes = depth > 0 ? perft(searchThread.mBoard, depth) : 0;

        std::cout << "perft depth " << depth 
                  << " nodes " << nodes 
                  << " nps " << nodes * 1000 / std::max((u64)millisecondsElapsed(start), (u64)1)
                  << " time " << millisecondsElapsed(start)
                  << " fen " << fen
                  << std::endl;
    }
    else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
    || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
    {
        const int depth = stoi(tokens[1]);
        
        std::cout << "perft split depth " << depth 
                  << " '" << searchThread.mBoard.fen() << "'"
                  << std::endl;

        if (depth <= 0) {
            std::cout << "Total: 0" << std::endl;
            return;
        }

        ArrayVec<Move, 256> moves;
        searchThread.mBoard.pseudolegalMoves(moves, MoveGenType::ALL);

        u64 totalNodes = 0;

        for (const Move move : moves) 
            if (searchThread.mBoard.isPseudolegalLegal(move))
            {
                searchThread.mBoard.makeMove(move);
                const u64 nodes = perft(searchThread.mBoard, depth - 1);
                std::cout << move.toUci() << ": " << nodes << std::endl;
                totalNodes += nodes;
                searchThread.mBoard.undoMove();
            }

        std::cout << "Total: " << totalNodes << std::endl;
    }
    else if (tokens[0] == "makemove")
    {
        if ((tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
        && !searchThread.mBoard.inCheck())
            searchThread.mBoard.makeMove(MOVE_NONE);
        else
            searchThread.mBoard.makeMove(tokens[1]);
    }
    else if (tokens[0] == "undomove")
        searchThread.mBoard.undoMove();
    #if defined(TUNE)
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
    #endif
}

inline void uci() {
    std::cout << "id name Starzix" << std::endl;
    std::cout << "id author zzzzz" << std::endl;
    std::cout << "option name Hash type spin default 32 min 1 max 65536" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 1" << std::endl;

    #if defined(TUNE)
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
    #endif

    std::cout << "uciok" << std::endl;
}

inline void setoption(const std::vector<std::string> &tokens, std::vector<TTEntry>* ttPtr)
{
    const std::string optionName  = tokens[2];
    const std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        resizeTT(ttPtr, stoll(optionValue));
        printTTSize(ttPtr);
    }
    else if (optionName == "Threads" || optionName == "threads")
    {
        const int numThreads = std::clamp(stoi(optionValue), 1, 256);
    }
    #if defined(TUNE)
    else if (tunableParams.count(optionName) > 0) 
    {
        auto tunableParam = tunableParams[optionName];

        std::visit([optionName, optionValue] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            myParam->value = std::stod(optionValue);

            if (optionName == stringify(lmrBaseQuiet) || optionName == stringify(lmrMultiplierQuiet)
            || optionName == stringify(lmrBaseNoisy) || optionName == stringify(lmrMultiplierNoisy))
                LMR_TABLE = getLmrTable();

            std::cout << optionName << " set to " << myParam->value << std::endl;
        }, tunableParam);
    }
    #endif
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

inline void go(const std::vector<std::string> &tokens, SearchThread &searchThread)
{
    const std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    i64 milliseconds = std::numeric_limits<i64>::max();
    [[maybe_unused]] i64 incrementMs = 0;
    [[maybe_unused]] i64 movesToGo = 0;
    bool isMoveTime = false;
    u8 maxDepth = MAX_DEPTH;
    i64 maxNodes = std::numeric_limits<i64>::max();

    for (int i = 1; i < int(tokens.size()) - 1; i += 2)
    {
        const i64 value = std::max<i64>(std::stoll(tokens[i + 1]), 0);

        if ((tokens[i] == "wtime" && searchThread.mBoard.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searchThread.mBoard.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searchThread.mBoard.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searchThread.mBoard.sideToMove() == Color::BLACK))
            incrementMs = value;

        else if (tokens[i] == "movestogo")
            movesToGo = std::max<i64>(value, 1);
        else if (tokens[i] == "movetime")
        {
            isMoveTime = true;
            milliseconds = value;
        }
        else if (tokens[i] == "depth")
            maxDepth = std::min<i64>(value, MAX_DEPTH);
        else if (tokens[i] == "nodes")
            maxNodes = value;
    }

    // Calculate search time limits

    i64 hardMilliseconds = std::max<i64>(0, milliseconds - 10);
    i64 softMilliseconds = std::numeric_limits<i64>::max();

    if (!isMoveTime) {
        hardMilliseconds *= hardTimePercentage();
        softMilliseconds = hardMilliseconds * softTimePercentage();
    }

    searchThread.search(maxDepth, maxNodes, startTime, hardMilliseconds, softMilliseconds, true);

    std::cout << "bestmove " << searchThread.bestMoveRoot().toUci() << std::endl;
}

} // namespace uci
