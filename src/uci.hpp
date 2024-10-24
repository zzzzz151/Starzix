// clang-format-off

#pragma once

#include "utils.hpp"
#include "board.hpp"
#include "search.hpp"
#include "bench.hpp"
#include "nnue.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(const std::vector<std::string> &tokens, Searcher &searcher);
inline void position(const std::vector<std::string> &tokens, Board &board);
inline void go(const std::vector<std::string> &tokens, Searcher &searcher);

inline bool runCommand(std::string &command, Searcher &searcher)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    if (command == "" || tokens.size() == 0)
        return true;
    // UCI commands
    else if (command == "uci")
        uci();
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
        setoption(tokens, searcher);
    else if (command == "ucinewgame")
        searcher.ucinewgame();
    else if (command == "isready")
        std::cout << "readyok" << std::endl;
    else if (tokens[0] == "position")
        position(tokens, searcher.board());
    else if (tokens[0] == "go")
        go(tokens, searcher);
    else if (command == "quit")
        return false;
    // Non-UCI commands
    else if (command == "print" || command == "d"
    || command == "display" || command == "show")
        searcher.board().print();
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
        const BothAccumulators acc = BothAccumulators(searcher.board());
        const i32 eval = nnue::evaluate(&acc, searcher.board().sideToMove());
        const i32 evalScaled = eval * materialScale(searcher.board());

        std::cout << "eval "    << eval
                  << " scaled " << evalScaled
                  << std::endl;
    }
    else if (tokens[0] == "perft")
    {
        const int depth = stoi(tokens[1]);
        const std::string fen = searcher.board().fen();

        std::cout << "perft depth " << depth << " '" << fen << "'" << std::endl;

        const std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
        const u64 nodes = depth > 0 ? perft(searcher.board(), depth) : 0;

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
                  << " '" << searcher.board().fen() << "'"
                  << std::endl;

        if (depth <= 0) {
            std::cout << "Total: 0" << std::endl;
            return true;
        }

        ArrayVec<Move, 256> moves;
        searcher.board().pseudolegalMoves(moves, MoveGenType::ALL);

        u64 totalNodes = 0;

        for (const Move move : moves) 
            if (searcher.board().isPseudolegalLegal(move))
            {
                searcher.board().makeMove(move);
                const u64 nodes = perft(searcher.board(), depth - 1);
                std::cout << move.toUci() << ": " << nodes << std::endl;
                totalNodes += nodes;
                searcher.board().undoMove();
            }

        std::cout << "Total: " << totalNodes << std::endl;
    }
    else if (tokens[0] == "makemove")
    {
        if ((tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
        && !searcher.board().inCheck())
            searcher.board().makeMove(MOVE_NONE);
        else
            searcher.board().makeMove(tokens[1]);
    }
    else if (tokens[0] == "undomove")
        searcher.board().undoMove();
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

    return true;
}

inline void uci() {
    std::cout << "id name Starzix 6.0" << std::endl;
    std::cout << "id author zzzzz" << std::endl;
    std::cout << "option name Hash type spin default 32 min 1 max 65536" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 256" << std::endl;

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

inline void setoption(const std::vector<std::string> &tokens, Searcher &searcher)
{
    const std::string optionName  = tokens[2];
    const std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        resizeTT(searcher.mTT, stoll(optionValue));
        printTTSize(searcher.mTT);
    }
    else if (optionName == "Threads" || optionName == "threads")
    {
        const int newNumThreads = searcher.setThreads(stoi(optionValue));
        std::cout << "info string Threads set to " << newNumThreads << std::endl;
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

            std::cout << "info string " << optionName << " set to " << myParam->value << std::endl;
        }, tunableParam);
    }
    #endif
}

inline void position(const std::vector<std::string> &tokens, Board &board)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos") {
        board = START_BOARD;
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

inline void go(const std::vector<std::string> &tokens, Searcher &searcher)
{
    const std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    i64 milliseconds = std::numeric_limits<i64>::max();
    [[maybe_unused]] i64 incrementMs = 0;
    [[maybe_unused]] i64 movesToGo = 0;
    bool isMoveTime = false;
    i32 maxDepth = MAX_DEPTH;
    i64 maxNodes = std::numeric_limits<i64>::max();

    for (int i = 1; i < int(tokens.size()) - 1; i += 2)
    {
        const i64 value = std::max<i64>(std::stoll(tokens[i + 1]), 0);

        if ((tokens[i] == "wtime" && searcher.board().sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searcher.board().sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searcher.board().sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searcher.board().sideToMove() == Color::BLACK))
            incrementMs = value;

        else if (tokens[i] == "movestogo")
            movesToGo = std::max<i64>(value, 1);
        else if (tokens[i] == "movetime")
        {
            isMoveTime = true;
            milliseconds = value;
        }
        else if (tokens[i] == "depth")
            maxDepth = value;
        else if (tokens[i] == "nodes")
            maxNodes = value;
    }

    // Calculate search time limits

    i64 hardMs = std::max<i64>(0, milliseconds - 10);
    i64 softMs = std::numeric_limits<i64>::max();

    if (!isMoveTime) {
        hardMs *= hardTimePercentage();
        softMs = hardMs * softTimePercentage();
    }

    const auto [bestMove, score] = searcher.search(maxDepth, maxNodes, startTime, hardMs, softMs, true);

    std::cout << "bestmove " << bestMove.toUci() << std::endl;
}

} // namespace uci
