// clang-format-off

#pragma once

#include "bench.hpp"
#include "perft.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(Searcher &searcher, std::vector<std::string> &tokens);
inline void ucinewgame(Searcher &searcher);
inline void position(Searcher &searcher, std::vector<std::string> &tokens);
inline void go(Searcher &searcher, std::vector<std::string> &tokens);

inline void uciLoop(Searcher &searcher)
{
    while (true)
    {
        searcher.resetLimits();

        std::string received = "";
        getline(std::cin, received);
        trim(received);
        std::vector<std::string> tokens = splitString(received, ' ');
        if (received == "" || tokens.size() == 0)
            continue;

        try {

        if (received == "quit" || !std::cin.good())
            break;
        else if (received == "uci")
            uci();
        else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(searcher, tokens);
        else if (received == "ucinewgame")
            ucinewgame(searcher);
        else if (received == "isready")
            std::cout << "readyok\n";
        else if (tokens[0] == "position")
            position(searcher, tokens);
        else if (tokens[0] == "go")
            go(searcher, tokens);
        else if (tokens[0] == "print" || tokens[0] == "d"
        || tokens[0] == "display" || tokens[0] == "show")
            searcher.board.print();
        else if (tokens[0] == "eval")
            std::cout << "eval " << searcher.board.evaluate() << " cp" << std::endl;
        else if (tokens[0] == "bench")
        {
            if (tokens.size() == 1)
                bench();
            else
            {
                u8 depth = stoi(tokens[1]);
                bench(depth);
            }
        }
        else if (tokens[0] == "perft")
        {
            int depth = stoi(tokens[1]);
            perft::perftBench(searcher.board, depth);
        }
        else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide")
        {
            int depth = stoi(tokens[1]);
            perft::perftSplit(searcher.board, depth);
        }
        else if (tokens[0] == "makemove")
        {
            if (tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
            {
                assert(!searcher.board.inCheck());
                searcher.board.makeMove(MOVE_NONE);
            }
            else
                searcher.board.makeMove(tokens[1]);
        }
        else if (tokens[0] == "undomove")
            searcher.board.undoMove();
        else if (received == "paramsjson")
            printParamsAsJson();

        } 
        catch (const char* errorMessage)
        {

        }
        
    }
}

inline void uci()
{
    std::cout << "id name Starzix\n";
    std::cout << "id author zzzzz\n";
    std::cout << "option name Hash type spin default " << TT_DEFAULT_SIZE_MB << " min 1 max 1024\n";

    /*
    for (auto &myTunableParam : tunableParams) 
    {
        std::visit([](auto &tunableParam) 
        {
            std::cout << "option name " << tunableParam->name;
            if (std::is_same<decltype(tunableParam->value), double>::value)
            {
                std::cout << " type spin default " << (tunableParam->value * 100.0)
                          << " min " << (tunableParam->min * 100.0)
                          << " max " << (tunableParam->max * 100.0);
            }
            else
            {
                std::cout << " type spin default " << (i64)tunableParam->value
                          << " min " << (i64)tunableParam->min
                          << " max " << (i64)tunableParam->max;
            }
            std::cout << "\n";
        }, myTunableParam);
    }
    */

    std::cout << "uciok" << std::endl;
}

inline void setoption(Searcher &searcher, std::vector<std::string> &tokens)
{
    std::string optionName = tokens[2];
    trim(optionName);
    std::string optionValue = tokens[4];
    trim(optionValue);

    if (optionName == "Hash" || optionName == "hash")
    {
        ttSizeMB = stoi(optionValue);
        searcher.tt.resize();
    }
    else
    {
        bool found = false;
        for (auto &myTunableParam : tunableParams) 
        {
            std::visit([optionName, optionValue, &found](auto &tunableParam) 
            {
                if (optionName == tunableParam->name)
                {
                    tunableParam->value = std::is_same<decltype(tunableParam->value), double>::value
                                          ? stoi(optionValue) / 100.0 : stoi(optionValue);

                    if (tunableParam->name == lmrBase.name 
                    || tunableParam->name == lmrMultiplier.name)
                        initLmrTable();

                    found = true;
                }
            }, myTunableParam);

            if (found) break;
        }
    }
}

inline void ucinewgame(Searcher &searcher)
{
    searcher.tt.reset(); // reset/clear TT

     // reset/clear histories and countermoves
    memset(searcher.historyTable.data(), 0, sizeof(searcher.historyTable));
    memset(searcher.countermoves.data(), 0, sizeof(searcher.countermoves));

    // reset/clear killer moves
    for (int i = 0; i < searcher.pliesData.size(); i++)
        searcher.pliesData[i].killer = MOVE_NONE;
}

inline void position(Searcher &searcher, std::vector<std::string> &tokens)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos")
    {
        searcher.board = START_BOARD;
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        int i = 0;
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";
        fen.pop_back(); // remove last whitespace
        searcher.board = Board(fen);
        movesTokenIndex = i;
    }

    for (int i = movesTokenIndex + 1; i < tokens.size(); i++)
        searcher.board.makeMove(tokens[i]);
}

inline void go(Searcher &searcher, std::vector<std::string> &tokens)
{
    u64 milliseconds = U64_MAX;
    u64 incrementMilliseconds = 0;
    u16 movesToGo = defaultMovesToGo.value;
    bool isMoveTime = false;

    for (int i = 1; i < (int)tokens.size() - 1; i += 2)
    {
        i64 value = stoi(tokens[i + 1]);

        if ((tokens[i] == "wtime" && searcher.board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searcher.board.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searcher.board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searcher.board.sideToMove() == Color::BLACK))
            incrementMilliseconds = value;

        else if (tokens[i] == "movestogo")
            movesToGo = value;
        else if (tokens[i] == "movetime")
        {
            milliseconds = value;
            isMoveTime = true;
        }
        else if (tokens[i] == "depth")
            searcher.maxDepth = value;
        else if (tokens[i] == "nodes")
            searcher.softNodes = searcher.hardNodes = value;
    }

    searcher.setTimeLimits(milliseconds, incrementMilliseconds, movesToGo, isMoveTime);
    auto [bestMove, score] = searcher.search();
    assert(bestMove != MOVE_NONE);
    std::cout << "bestmove " + bestMove.toUci() + "\n";
}

} // namespace uci


