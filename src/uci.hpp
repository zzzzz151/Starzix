#ifndef UCI_HPP
#define UCI_HPP

#include <cstring> // for memset()
#include "chess.hpp"
#include "search.hpp"
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
        for (int i = 2; i < 8; i++)
            fen += words[i];
        board = Board(fen);
        movesTokenIndex = 8;
    }

    for (int i = movesTokenIndex + 1; i < words.size(); i++)
        board.makeMove(uci::uciToMove(board, words[i]));
}

inline void uciLoop()
{
    string received;
    getline(cin, received);
    cout << "id name z5\n";
    cout << "id author zzzzz\n";
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
        else if (received == "ucinewgame")
        {
            // memset(TT, 0, sizeof(TT)); // clear TT
            for (int i = 0; i < 0x800000; i++)
                TT[i].key = 0;
        }
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
            position(words);
        else if (words[0] == "go")
        {
            // clear killers
            // memset(killerMoves, 0, sizeof(killerMoves));
            for (int i = 0; i < 512; i++)
            {
                killerMoves[i][0] = NULL_MOVE;
                killerMoves[i][1] = NULL_MOVE;
            }

            // clear history moves
            memset(&historyMoves[0][0][0], 0, sizeof(historyMoves));

            int millisecondsLeft = 60000;
            if (words[1] == "wtime")
                millisecondsLeft = board.sideToMove() == Color::WHITE ? stoi(words[2]) : stoi(words[4]);
            else if (words[1] == "movetime")
                millisecondsLeft = stoi(words[2]);

            int depthReached = iterativeDeepening(millisecondsLeft);
            // cout << "depthReached " << depthReached << endl;
            cout << "bestmove " + uci::moveToUci(bestMoveRootAsp == NULL_MOVE ? bestMoveRoot : bestMoveRootAsp) + "\n";
        }
    }
}

#endif
