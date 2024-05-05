// clang-format-off

#pragma once

#include "bench.hpp"
#include "perft.hpp"

namespace uci { // Universal chess interface

inline void uci();
inline void setoption(Searcher &searcher, std::vector<std::string> &tokens);
inline void ucinewgame(Searcher &searcher);
inline void position(Board &board, std::vector<std::string> &tokens);
inline void go(Searcher &searcher, std::vector<std::string> &tokens);

inline void uciLoop()
{
    Searcher searcher = Searcher();
    searcher.printTTSize();

    while (true) {
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
            searcher.ucinewgame();
        else if (received == "isready")
            std::cout << "readyok\n";
        else if (tokens[0] == "position")
            position(searcher.board, tokens);
        else if (tokens[0] == "go")
            go(searcher, tokens);
        else if (tokens[0] == "print" || tokens[0] == "d"
        || tokens[0] == "display" || tokens[0] == "show")
            searcher.board.print();
        else if (tokens[0] == "eval") {
            Accumulator acc = Accumulator(searcher.board);
            std::cout << "eval " << evaluate(acc, searcher.board, false)
                      << " scaled "  << evaluate(acc, searcher.board, true)
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
            perftBench(searcher.board, depth);
        }
        else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
        {
            int depth = stoi(tokens[1]);
            perftSplit(searcher.board, depth);
        }
        else if (tokens[0] == "makemove")
        {
            if ((tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
            && !searcher.board.inCheck())
                searcher.board.makeMove(MOVE_NONE);
            else
                searcher.board.makeMove(tokens[1]);
        }
        else if (tokens[0] == "undomove")
            searcher.board.undoMove();
        else if (tokens[0] == "islegal")
        {
            Move move = searcher.board.uciToMove(tokens[1]);
            u64 pinned = searcher.board.pinned();
            std::cout << searcher.board.isPseudolegalLegal(move, pinned) << std::endl;
        }
        else if (tokens[0] == "moves") {
            MovesList moves = MovesList();
            searcher.board.pseudolegalMoves(moves);
            std::cout << "Pseudolegals: " << (int)moves.size() << std::endl;

            u8 legals = 0;
            u64 pinned = searcher.board.pinned();
            for (int i = 0; i < moves.size(); i++)
                legals += searcher.board.isPseudolegalLegal(moves[i], pinned);

            std::cout << "Legals: " << (int)legals << std::endl;
        }
        else if (received == "paramsjson")
            printParamsAsJson();

        } 
        catch (const char* errorMessage)
        {

        }
    }
}

inline void uci() {
    std::cout << "id name Starzix\n";
    std::cout << "id author zzzzz\n";
    std::cout << "option name Hash type spin default 32 min 1 max 1024\n";

    /*
    for (auto &myTunableParam : tunableParams) 
    {
        std::visit([](auto &tunableParam) 
        {
            std::cout << "option name " << tunableParam->name;
            if (std::is_same<decltype(tunableParam->value), double>::value
            || std::is_same<decltype(tunableParam->value), float>::value)
            {
                std::cout << " type spin default " << i64(tunableParam->value * 100.0)
                          << " min " << i64(tunableParam->min * 100.0)
                          << " max " << i64(tunableParam->max * 100.0);
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
        searcher.resizeTT(stoll(optionValue));
        searcher.printTTSize();
    }
    else {
        bool found = false;
        for (auto &myTunableParam : tunableParams) 
        {
            std::visit([optionName, optionValue, &found](auto &tunableParam) 
            {
                if (optionName == tunableParam->name)
                {
                    tunableParam->value = std::is_same<decltype(tunableParam->value), double>::value
                                          || std::is_same<decltype(tunableParam->value), float>::value
                                          ? stoll(optionValue) / 100.0 : stoll(optionValue);

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

inline void position(Board &board, std::vector<std::string> &tokens)
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

inline void go(Searcher &searcher, std::vector<std::string> &tokens)
{
    u8 maxDepth = MAX_DEPTH;
    i64 milliseconds = I64_MAX;
    u64 incrementMs = 0;
    u64 movesToGo = defaultMovesToGo.value;
    bool isMoveTime = false;
    u64 maxNodes = U64_MAX;

    for (int i = 1; i < (int)tokens.size() - 1; i += 2)
    {
        i64 value = std::stoll(tokens[i + 1]);

        if ((tokens[i] == "wtime" && searcher.board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && searcher.board.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && searcher.board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && searcher.board.sideToMove() == Color::BLACK))
            incrementMs = value;

        else if (tokens[i] == "movestogo")
            movesToGo = value;
        else if (tokens[i] == "movetime")
        {
            milliseconds = value;
            isMoveTime = true;
        }
        else if (tokens[i] == "depth")
            maxDepth = std::clamp(value, (i64)1, (i64)MAX_DEPTH);
        else if (tokens[i] == "nodes")
            maxNodes = value;
    }

    auto [bestMove, score] = searcher.search(maxDepth, milliseconds, incrementMs, movesToGo, 
                                             isMoveTime, maxNodes, maxNodes, true);

    assert(bestMove != MOVE_NONE);
    std::cout << "bestmove " + bestMove.toUci() + "\n";
}

} // namespace uci


