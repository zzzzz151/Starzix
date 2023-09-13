#ifndef UCI_HPP
#define UCI_HPP

// clang-format-off
#include "chess.hpp"
#include "main.cpp"
inline void initTT();
#include "nnue.hpp"
#include "search.hpp"
#include <cstring> // for memset()
using namespace chess;
using namespace std;

inline void setoption(vector<string> &words) // e.g. "setoption name Hash value 32"
{
    string optionName = words[2];
    string optionValue = words[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        TT_SIZE_MB = stoi(optionValue);
        initTT();
    }
}

inline void ucinewgame()
{
    // clear TT
    memset(TT.data(), 0, sizeof(TTEntry) * TT.size());

    // clear killers
    int numRows = sizeof(killerMoves) / sizeof(killerMoves[0]);
    for (int i = 0; i < numRows; i++)
    {
        killerMoves[i][0] = NULL_MOVE;
        killerMoves[i][1] = NULL_MOVE;
    }
}

inline void position(vector<string> &words)
{
    int movesTokenIndex = -1;

    if (words[1] == "startpos")
    {
        board = Board(STARTPOS);
        movesTokenIndex = 2;
    }
    else if (words[1] == "fen")
    {
        string fen = "";
        int i = 0;
        for (i = 2; i < words.size() && words[i] != "moves"; i++)
            fen += words[i] + " ";
        fen.pop_back(); // remove last whitespace
        board = Board(fen);
        movesTokenIndex = i;
    }

    for (int i = movesTokenIndex + 1; i < words.size(); i++)
    {
        Move move = uci::uciToMove(board, words[i]);
        board.makeMove(move);
    }
}

inline void go(vector<string> &words)
{
    Move bestMove = iterativeDeepening(words);
    cout << "bestmove " + uci::moveToUci(bestMove) + "\n";
}

inline void info(int depth, int score)
{
    double millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
    double nps = millisecondsElapsed == 0 ? nodes : nodes / millisecondsElapsed * 1000.0;

    cout << "info depth " << depth
         << " time " << round(millisecondsElapsed)
         << " nodes " << nodes
         << " nps " << (U64)round(nps)
         << " score cp " << score
         << " pv " << uci::moveToUci(bestMoveRoot)
         << endl;
}

inline void uciLoop()
{
    string received;
    getline(cin, received);
    cout << "id name test\n";
    cout << "id author zzzzz\n";
    cout << "option name Hash type spin default " << TT_SIZE_MB << " min 1 max 512\n";
    cout << "uciok\n";

    while (true)
    {
        getline(cin, received);
        istringstream stringStream(received);
        vector<string> words;
        string word;
        while (getline(stringStream, word, ' '))
            words.push_back(word);

        if (received == "quit" || !cin.good())
            break;
        else if (words[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(words);
        else if (received == "ucinewgame")
            ucinewgame();
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
            position(words);
        else if (words[0] == "go")
            go(words);
        else if (words[0] == "eval")
            cout << "eval " << network.Evaluate((int)board.sideToMove()) << " cp" << endl;
    }
}

#endif
