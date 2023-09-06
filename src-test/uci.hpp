#ifndef UCI_HPP
#define UCI_HPP

#include <cstring> // for memset()
#include "chess.hpp"
#include "search.hpp"
#include "nnue.hpp"
using namespace chess;
using namespace std;

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
    cout << "option name Hash type spin default 64 min 1 max 512\n";
    cout << "option name Info type check default false\n";
    cout << "uciok\n";

    NUM_TT_ENTRIES = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
    TT.clear();
    TT.resize(NUM_TT_ENTRIES);
    // cout << "TT_SIZE_MB = " << TT_SIZE_MB << " => " << NUM_TT_ENTRIES << " entries" << endl;

    bool info = false;

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
        else if (words[0] == "setoption") // setoption name Hash value 32
        {
            string optionName = words[2];
            string optionValue = words[4];
            if (optionName == "Hash")
            {
                TT_SIZE_MB = stoi(optionValue);
                NUM_TT_ENTRIES = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
                TT.clear();
                TT.resize(NUM_TT_ENTRIES);
                // cout << "TT_SIZE_MB = " << TT_SIZE_MB << " => " << NUM_TT_ENTRIES << " entries" << endl;
            }
            else if (optionName == "Info")
            {
                if (optionValue == "true" || optionValue == "True")
                    info = true;
                else if (optionValue == "false" || optionValue == "False")
                    info = false;
            }
        }
        else if (received == "ucinewgame")
        {
            // memset(TT, 0, sizeof(TT)); // clear TT
            // for (int i = 0; i < 0x800000; i++)
            //    TT[i].key = 0;
            memset(TT.data(), 0, sizeof(TTEntry) * TT.size());
        }
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
        {
            position(words);
        }
        else if (words[0] == "go")
        {
            start = chrono::steady_clock::now();

            // clear killers
            // memset(killerMoves, 0, sizeof(killerMoves));
            for (int i = 0; i < 512; i++)
            {
                killerMoves[i][0] = NULL_MOVE;
                killerMoves[i][1] = NULL_MOVE;
            }

            // clear history moves
            memset(&historyMoves[0][0][0], 0, sizeof(historyMoves));

            millisecondsForThisTurn = 60000;
            if (words[1] == "wtime")
            {
                U64 millisecondsLeft = board.sideToMove() == Color::WHITE ? stoi(words[2]) : stoi(words[4]);
                millisecondsForThisTurn = millisecondsLeft / 30.000;
            }
            else if (words[1] == "movetime")
                millisecondsForThisTurn = stoi(words[2]);

            Move bestMove = iterativeDeepening(info);
            cout << "bestmove " + uci::moveToUci(bestMove) + "\n";
        }
        else if (words[0] == "eval")
        {
            cout << "eval: " << network.Evaluate((int)board.sideToMove()) << " cp" << endl;
        }
    }
}

#endif
