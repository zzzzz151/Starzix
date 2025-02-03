// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search.hpp"

namespace uci {

constexpr void position(const std::vector<std::string>& tokens, Position& pos);
constexpr void go(const Position& pos, Searcher& searcher);

inline void runCommand(std::string& command, Position& pos, Searcher& searcher)
{
    trim(command);
    const std::vector<std::string> tokens = splitString(command, ' ');

    if (command == "" || tokens.size() == 0)
        return;

    // UCI commands
    if (command == "uci")
    {
        std::cout << "id name Starzix";
        std::cout << "\nid author zzzzz";
        std::cout << "\noption name Hash type spin default 32 min 1 max 65536";
        std::cout << "\noption name Threads type spin default 1 min 1 max 256";
        std::cout << "\nuciok";
        std::cout << std::endl; // flush
    }
    else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
    {

    }
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
        go(pos, searcher);
    else if (command == "quit")
    {
        exit(EXIT_SUCCESS);
    }
    // Non-UCI commands
    else if (command == "d" || command == "display"
    || command == "show" || command == "print")
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

        fen.pop_back(); // remove last whitespace
        pos = Position(fen);
        movesTokenIndex = i;
    }

    for (size_t i = movesTokenIndex + 1; i < tokens.size(); i++)
        pos.makeMove(tokens[i]);
}

constexpr void go(const Position& pos, Searcher& searcher)
{
    const SearchConfig searchConfig = SearchConfig();
    searcher.search(pos, searchConfig);
    std::cout << "bestmove " << searcher.bestMoveAtRoot().toUci() << std::endl;
}

} // namespace uci
