#ifndef UCI_HPP
#define UCI_HPP

// clang-format-off
#include <cstring> // for memset()
#include <chrono>
#include "board/board.hpp"
#include "nnue.hpp"
#include "search.hpp"
inline void initTT();
using namespace std;

namespace uci
{

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
    }

    inline void position(vector<string> &words)
    {
        int movesTokenIndex = -1;

        if (words[1] == "startpos")
        {
            board = Board(START_FEN);
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
            board.makeMove(words[i]);
    }

    inline void go(vector<string> &words)
    {
        setupTime(words, board.colorToMove());
        Move bestMove = iterativeDeepening();
        cout << "bestmove " + bestMove.toUci() + "\n";
    }

    inline void info(int depth, int score)
    {
        bool isMate = abs(score) >= MIN_MATE_SCORE;
        int movesToMate = 0;
        if (isMate)
        {
            int pliesToMate = POS_INFINITY - abs(score);
            movesToMate = round(pliesToMate / 2.0);
            if (score < 0) movesToMate *= -1; // we are getting mated
        }

        double millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
        uint64_t nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1) * 1000;

        cout << "info depth " << depth
            << " seldepth " << maxPlyReached
            << " time " << round(millisecondsElapsed)
            << " nodes " << nodes
            << " nps " << nps
            << (isMate ? " score mate " : " score cp ") << (isMate ? movesToMate : score)
            << " pv " << bestMoveRoot.toUci()
            << endl;
    }

    inline void uciLoop()
    {
        string received;
        getline(cin, received);
        cout << "id name z5_tests\n";
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
                cout << "eval " << nnue::evaluate(board.colorToMove()) << " cp" << endl;
        }
    }

}

#endif
