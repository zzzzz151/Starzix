// clang-format-off

#pragma once

#include "utils.hpp"
#include "board.hpp"
#include "searcher.hpp"
#include "bench.hpp"
#include "nnue.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(const std::vector<std::string> &tokens, Searcher &searcher);
inline void position(const std::vector<std::string> &tokens, Board &board);
inline void go(const std::vector<std::string> &tokens, Searcher &searcher);

inline void runCommand(std::string &command, Searcher &searcher)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    Board &board = searcher.mainThreadData()->board;

    if (command == "" || tokens.size() == 0)
        return;
    // UCI commands
    else if (command == "uci")
        uci();
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
        setoption(tokens, searcher);
    else if (command == "ucinewgame")
        searcher.reset();
    else if (command == "isready")
        std::cout << "readyok" << std::endl;
    else if (tokens[0] == "position")
        position(tokens, board);
    else if (tokens[0] == "go")
        go(tokens, searcher);
    else if (command == "quit")
        exit(EXIT_SUCCESS);
    // Non-UCI commands
    else if (command == "print" || command == "d"
    || command == "display" || command == "show")
        board.print();
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
        const BothAccumulators acc = BothAccumulators(board);
        const i32 eval = nnue::evaluate(&acc, board.sideToMove());
        const i32 evalScaled = eval * materialScale(board);

        std::cout << "eval "    << eval
                  << " scaled " << evalScaled
                  << std::endl;
    }
    else if (tokens[0] == "perft")
    {
        const int depth = stoi(tokens[1]);
        const std::string fen = board.fen();

        std::cout << "perft depth " << depth << " '" << fen << "'" << std::endl;

        const std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
        const u64 nodes = depth > 0 ? perft(board, depth) : 0;

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
                  << " '" << board.fen() << "'"
                  << std::endl;

        if (depth <= 0) {
            std::cout << "Total: 0" << std::endl;
            return;
        }

        ArrayVec<Move, 256> moves;
        board.pseudolegalMoves(moves, MoveGenType::ALL);

        u64 totalNodes = 0;

        for (const Move move : moves) 
            if (board.isPseudolegalLegal(move))
            {
                board.makeMove(move);
                const u64 nodes = perft(board, depth - 1);
                std::cout << move.toUci() << ": " << nodes << std::endl;
                totalNodes += nodes;
                board.undoMove();
            }

        std::cout << "Total: " << totalNodes << std::endl;
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

    const Board &board = searcher.mainThreadData()->board;

    i64 milliseconds = std::numeric_limits<i64>::max();
    [[maybe_unused]] i64 incrementMs = 0;
    [[maybe_unused]] i64 movesToGo = 0;
    bool isMoveTime = false;
    i32 maxDepth = MAX_DEPTH;
    i64 maxNodes = std::numeric_limits<i64>::max();

    for (int i = 1; i < int(tokens.size()) - 1; i += 2)
    {
        const i64 value = std::max<i64>(std::stoll(tokens[i + 1]), 0);

        if ((tokens[i] == "wtime" && board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && board.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && board.sideToMove() == Color::BLACK))
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

    std::cout << "bestmove " << searcher.bestMoveRoot().toUci() << std::endl;
}

} // namespace uci
