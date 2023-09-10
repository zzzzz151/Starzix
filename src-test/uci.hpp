#ifndef UCI_HPP
#define UCI_HPP

#include <cstring> // for memset()
#include "chess.hpp"
#include "search.hpp"
#include "nnue.hpp"
using namespace chess;
using namespace std;

int numRowsOfKillerMoves = sizeof(killerMoves) / sizeof(killerMoves[0]);
bool info = false;

inline void setoption(vector<string> &words) // e.g. "setoption name Hash value 32"
{
    string optionName = words[2];
    string optionValue = words[4];

    if (optionName == "Hash" || optionName == "hash")
    {
        TT_SIZE_MB = stoi(optionValue);
        NUM_TT_ENTRIES = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
        TT.clear();
        TT.resize(NUM_TT_ENTRIES);
        // cout << "TT_SIZE_MB = " << TT_SIZE_MB << " => " << NUM_TT_ENTRIES << " entries" << endl;
    }
    else if (optionName == "Info" || optionName == "info")
    {
        if (optionValue == "true" || optionValue == "True")
            info = true;
        else if (optionValue == "false" || optionValue == "False")
            info = false;
    }
}

inline void ucinewgame()
{
    // clear TT
    memset(TT.data(), 0, sizeof(TTEntry) * TT.size());

    // clear killers
    for (int i = 0; i < numRowsOfKillerMoves; i++)
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
    start = chrono::steady_clock::now();

    // clear history moves
    memset(&historyMoves[0][0][0], 0, sizeof(historyMoves));

    int milliseconds = DEFAULT_TIME_MILLISECONDS;
    byte timeType = (byte)-1;

    if (words[1] == "wtime")
    {
        milliseconds = board.sideToMove() == Color::WHITE ? stoi(words[2]) : stoi(words[4]);
        timeType = TIME_TYPE_NORMAL_GAME;
    }
    else if (words[1] == "movetime")
    {
        milliseconds = stoi(words[2]);
        timeType = TIME_TYPE_MOVE_TIME;
    }

    Move bestMove = iterativeDeepening(milliseconds, timeType, info);
    cout << "bestmove " + uci::moveToUci(bestMove) + "\n";
}

inline void sendInfo(int depth, int score)
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
    cout << "option name Info type check default false\n";
    cout << "uciok\n";

    NUM_TT_ENTRIES = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
    TT.clear();
    TT.resize(NUM_TT_ENTRIES);
    // cout << "TT_SIZE_MB = " << TT_SIZE_MB << " => " << NUM_TT_ENTRIES << " entries" << endl;

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
