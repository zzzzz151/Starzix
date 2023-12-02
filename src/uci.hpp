#pragma once

// clang-format-off

#include "bench.hpp"
#include "perft.hpp"

namespace uci { // Universal chess interface

bool outputSearchInfo = true;

inline void setoption(std::vector<std::string> &tokens) // e.g. "setoption name Hash value 32"
{
    std::string optionName = tokens[2];
    std::string optionValue = tokens[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        int ttSizeMB = stoi(optionValue);
        tt::resize(ttSizeMB);
    }
}

inline void ucinewgame()
{
    tt::reset();
    memset(search::historyTable, 0, sizeof(search::historyTable)); // reset/clear histories
    memset(search::killerMoves, 0, sizeof(search::killerMoves));   // reset/clear killer moves
    memset(search::countermoves, 0, sizeof(search::countermoves)); // reset/clear countermoves
}

inline void position(std::vector<std::string> &tokens)
{
    int movesTokenIndex = -1;

    if (tokens[1] == "startpos")
    {
        board = Board(START_FEN);
        movesTokenIndex = 2;
    }
    else if (tokens[1] == "fen")
    {
        std::string fen = "";
        int i = 0;
        for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
            fen += tokens[i] + " ";
        fen.pop_back(); // remove last whitespace
        board = Board(fen);
        movesTokenIndex = i;
    }

    for (int i = movesTokenIndex + 1; i < tokens.size(); i++)
        board.makeMove(tokens[i]);
}

inline void go(std::vector<std::string> &tokens)
{
    i64 milliseconds = -1,
        incrementMilliseconds = 0,
        movesToGo = -1;
    bool isMoveTime = false;
    u64 softNodes = U64_MAX, hardNodes = U64_MAX;
    u8 maxDepth = search::MAX_DEPTH;

    for (int i = 1; i < tokens.size() - 1; i += 2)
    {
        i64 value = stoi(tokens[i + 1]);

        if ((tokens[i] == "wtime" && board.sideToMove() == Color::WHITE) 
        ||  (tokens[i] == "btime" && board.sideToMove() == Color::BLACK))
            milliseconds = value;

        else if ((tokens[i] == "winc" && board.sideToMove() == Color::WHITE) 
        ||       (tokens[i] == "binc" && board.sideToMove() == Color::BLACK))
            incrementMilliseconds = value;

        else if (tokens[i] == "movestogo")
            movesToGo = value;
        else if (tokens[i] == "movetime")
        {
            milliseconds = value;
            isMoveTime = true;
        }
        else if (tokens[i] == "depth")
            maxDepth = min(value, 255);
        else if (tokens[i] == "nodes")
            softNodes = hardNodes = value;

    }

    TimeManager timeManager = TimeManager(milliseconds, incrementMilliseconds, movesToGo, 
                                          isMoveTime, softNodes, hardNodes);
    auto [bestMove, score] = search::search(timeManager, maxDepth);
    std::cout << "bestmove " + bestMove.toUci() + "\n";
}

inline void info(int depth, i16 score)
{
    if (!outputSearchInfo) return;

    auto millisecondsElapsed = search::timeManager.millisecondsElapsed();
    u64 nps = search::nodes * 1000 / (millisecondsElapsed > 0 ? millisecondsElapsed : 1);

    // "score cp <score>" or "score mate <moves>" ?
    bool isMate = abs(score) >= MIN_MATE_SCORE;
    int movesToMate = 0;
    if (isMate)
    {
        int pliesToMate = POS_INFINITY - abs(score);
        movesToMate = round(pliesToMate / 2.0);
        if (score < 0) movesToMate *= -1; // we are getting mated
    }

    // Collect PV
    std::string strPv = search::pvLines[0][0].toUci();
    for (int i = 1; i < search::pvLengths[0]; i++)
        strPv += " " + search::pvLines[0][i].toUci();

    std::cout << "info depth " << depth
        << " seldepth " << search::maxPlyReached
        << " time " << round(millisecondsElapsed)
        << " nodes " << search::nodes
        << " nps " << nps
        << (isMate ? " score mate " : " score cp ") << (isMate ? movesToMate : score)
        << " pv " << strPv
        << std::endl;
}

inline void uciLoop()
{
    std::string received = "";
    while (received != "uci")
    {
        getline(std::cin, received);
        trim(received);
    }

    std::cout << "id name z5\n";
    std::cout << "id author zzzzz\n";
    std::cout << "option name Hash type spin default " << tt::DEFAULT_SIZE_MB << " min 1 max 1024\n";
    std::cout << "uciok\n";

    while (true)
    {
        getline(std::cin, received);
        trim(received);
        std::vector<std::string> tokens = splitString(received, ' ');
        if (received == "" || tokens.size() == 0)
            continue;

        try {

        if (received == "quit" || !std::cin.good())
            break;
        else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(tokens);
        else if (received == "ucinewgame")
            ucinewgame();
        else if (received == "isready")
            std::cout << "readyok\n";
        else if (tokens[0] == "position")
            position(tokens);
        else if (tokens[0] == "go")
            go(tokens);
        else if (tokens[0] == "eval")
            std::cout << "eval " << nnue::evaluate(board.sideToMove()) << " cp" << std::endl;
        else if (tokens[0] == "bench")
        {
            u8 depth = tokens.size() > 1 ? stoi(tokens[1]) : bench::DEFAULT_DEPTH;
            bench::bench(depth);
        }
        else if (tokens[0] == "perft")
        {
            int depth = stoi(tokens[1]);
            perft::perftBench(board, depth);
        }
        else if (tokens[0] == "perftsplit")
        {
            int depth = stoi(tokens[1]);
            perft::perftSplit(board, depth);
        }

        } 
        catch (const char* errorMessage)
        {

        }
        
    }
}

}


