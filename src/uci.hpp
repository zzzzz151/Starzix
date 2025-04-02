// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search.hpp"
#include "bench.hpp"

namespace uci {

inline void uci();

inline void setoption(const std::vector<std::string>& tokens, Searcher& searcher);

constexpr void position(const std::vector<std::string>& tokens, Position& pos);

constexpr void go(
    const std::vector<std::string>& tokens, Position& pos, Searcher& searcher);

inline void runCommand(std::string& command, Position& pos, Searcher& searcher)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    if (command == "" || tokens.size() == 0)
        return;

    // UCI commands
    if (command == "uci")
        uci();
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
        setoption(tokens, searcher);
    else if (command == "ucinewgame")
    {
        pos = START_POS;
        searcher.ucinewgame();
    }
    else if (tokens[0] == "position")
        position(tokens, pos);
    else if (command == "isready")
        std::cout << "readyok" << std::endl;
    else if (tokens[0] == "go")
        go(tokens, pos, searcher);
    else if (command == "quit")
        exit(EXIT_SUCCESS);
    // Non-UCI commands
    else if (command == "d"
    || command == "display"
    || command == "show"
    || command == "print")
    {
        pos.print();
    }
    else if (tokens[0] == "perft" && tokens.size() == 2)
    {
        const i32 depth = stoi(tokens[1]);

        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        const u64 nodes = perft(pos, depth);
        const u64 nps = getNps(nodes, millisecondsElapsed(start));

        std::cout << nodes << " nodes " << nps << " nps" << std::endl;
    }
    else if ((tokens[0] == "perftsplit"
    || tokens[0] == "splitperft"
    || tokens[0] == "perftdivide"
    || tokens[0] == "divideperft")
    && tokens.size() == 2)
    {
        const i32 depth = stoi(tokens[1]);

        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        const u64 nodes = perftSplit(pos, depth);
        const u64 nps = getNps(nodes, millisecondsElapsed(start));

        std::cout << nodes << " nodes " << nps << " nps" << std::endl;
    }
    else if (tokens[0] == "bench" || tokens[0] == "benchmark")
    {
        if (tokens.size() > 1)
        {
            const i32 depth = stoi(tokens[1]);
            bench(depth);
        }
        else
            bench();
    }
    else if (command == "eval" || command == "evaluate" || command == "evaluation")
    {
        nnue::BothAccumulators bothAccs = nnue::BothAccumulators(pos);
        std::cout << "eval " << nnue::evaluate(bothAccs, pos.sideToMove()) << std::endl;
    }
    else if (tokens[0] == "makemove" && tokens.size() == 2)
        pos.makeMove(tokens[1]);
    else if (command == "undomove" && pos.lastMove())
        pos.undoMove();
    else if (command == "movepicker")
    {
        const std::unique_ptr<HistoryTable> historyTable = std::make_unique<HistoryTable>();

        const auto printMoves = [&] (const bool noisiesOnly) constexpr
        {
            MovePicker mp = MovePicker(noisiesOnly, MOVE_NONE, MOVE_NONE);

            while (true) {
                const auto [move, moveScore] = mp.nextLegal(pos, *historyTable);

                if (!move) break;

                std::cout << move.toUci() << ": " << moveScore << std::endl;
            }
        };

        std::cout << "All moves" << std::endl;
        printMoves(false);

        std::cout << "\nNoisy moves" << std::endl;
        printMoves(true);
    }
    #if defined(TUNE)
    else if (command == "spsainput")
        printSpsaInput();
    #endif
}

inline void uci()
{
    std::cout << "id name Starzix";
    std::cout << "\nid author zzzzz";
    std::cout << "\noption name Hash type spin default 32 min 1 max 131072";
    std::cout << "\noption name Threads type spin default 1 min 1 max 256";

    #if defined(TUNE)
        for (const auto& pair : tunableParams)
        {
            const std::string paramName = pair.first;
            const auto& tunableParam = pair.second;

            std::visit([&paramName] (const auto* myParam)
            {
                if (myParam == nullptr) return;

                std::cout << "\noption name " << paramName
                          << " type string"
                          << " default " << myParam->value
                          << " min "     << myParam->min
                          << " max "     << myParam->max;

            }, tunableParam);
        }
    #endif

    std::cout << "\nuciok" << std::endl;
}

inline void setoption(const std::vector<std::string>& tokens, Searcher& searcher)
{
    const std::string optionName  = tokens[2];
    const std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        const i64 newMebibytes = std::max<i64>(stoll(optionValue), 1);
        resizeTT(searcher.mTT, static_cast<size_t>(newMebibytes));
        printTTSize(searcher.mTT);
    }
    else if (optionName == "Threads" || optionName == "threads")
    {
        const i64 newNumThreads = std::max<i64>(stoll(optionValue), 1);
        searcher.setThreads(static_cast<size_t>(newNumThreads));
        std::cout << "info string Threads set to " << newNumThreads << std::endl;
    }
    #if defined(TUNE)
    else if (tunableParams.count(optionName) > 0)
    {
        const auto tunableParam = tunableParams[optionName];

        std::visit([optionName, optionValue] (auto* myParam)
        {
            if (myParam == nullptr) return;

            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wfloat-conversion"
            #pragma clang diagnostic ignored "-Wimplicit-float-conversion"
            #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"

            myParam->value = myParam->isFloatOrDouble()
                           ? std::stod(optionValue)
                           : std::stoi(optionValue);

            #pragma clang diagnostic pop

            if (optionName == stringify(lmrBaseNoisy)
            ||  optionName == stringify(lmrBaseQuiet)
            ||  optionName == stringify(lmrMulNoisy)
            ||  optionName == stringify(lmrMulQuiet))
                LMR_TABLE = getLmrTable();

            std::cout << "info string " << optionName
                      << " set to "     << myParam->value
                      << std::endl;
        }, tunableParam);
    }
    #endif
}

constexpr void position(const std::vector<std::string>& tokens, Position& pos)
{
    size_t movesTokenIndex = 0;

    if (tokens[1] == "startpos")
    {
        pos = START_POS;
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        size_t i = 0;

        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";

        fen.pop_back(); // Remove last whitespace
        pos = Position(fen);
        movesTokenIndex = i;
    }

    for (size_t i = movesTokenIndex + 1; i < tokens.size(); i++)
        pos.makeMove(tokens[i]);
}

constexpr void go(
    const std::vector<std::string>& tokens, Position& pos, Searcher& searcher)
{
    SearchConfig searchConfig = SearchConfig();

    [[maybe_unused]] u64 incrementMs = 0;
    bool isMoveTime = false;

    for (size_t i = 1; i < tokens.size() - 1; i += 2)
    {
        const u64 value = static_cast<u64>(std::max<i64>(std::stoll(tokens[i + 1]), 0));

        if ((tokens[i] == "wtime" && pos.sideToMove() == Color::White)
        ||  (tokens[i] == "btime" && pos.sideToMove() == Color::Black))
            searchConfig.hardMs = value;

        else if ((tokens[i] == "winc" && pos.sideToMove() == Color::White)
        ||       (tokens[i] == "binc" && pos.sideToMove() == Color::Black))
            incrementMs = value;

        else if (tokens[i] == "movestogo")
        {

        }
        else if (tokens[i] == "movetime")
        {
            isMoveTime = true;
            searchConfig.hardMs = value;
        }
        else if (tokens[i] == "depth")
            searchConfig.setMaxDepth(static_cast<i32>(value));
        else if (tokens[i] == "nodes")
            searchConfig.maxNodes = value;
    }

    if (searchConfig.hardMs)
    {
        // Remove move overheard milliseconds from hard time limit
        searchConfig.hardMs = static_cast<u64>(std::max<i64>(
            static_cast<i64>(*(searchConfig.hardMs)) - 20,
            0
        ));

        if (!isMoveTime)
        {
            searchConfig.hardMs = static_cast<u64>(llround(
                static_cast<double>(*(searchConfig.hardMs)) * timeHardPercentage()
            ));

            searchConfig.softMs = static_cast<u64>(llround(
                static_cast<double>(*(searchConfig.hardMs)) * timeSoftPercentage()
            ));
        }
    }

    const Move bestMove = searcher.search(pos, searchConfig);

    std::cout << "bestmove " << bestMove.toUci() << std::endl;
}

} // namespace uci
