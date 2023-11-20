#pragma once

// clang-format-off

#include "perft.hpp"

namespace uci
{

    inline void setoption(vector<string> &tokens) // e.g. "setoption name Hash value 32"
    {
        string optionName = tokens[2];
        string optionValue = tokens[4];

        if (optionName == "Hash" || optionName == "hash")
        {
            int ttSizeMB = stoi(optionValue);
            resizeTT(ttSizeMB);
        }
    }

    inline void ucinewgame()
    {
        clearTT();
        memset(historyTable, 0, sizeof(historyTable)); // clear histories
        memset(killerMoves, 0, sizeof(killerMoves));   // clear killer moves
        memset(countermoves, 0, sizeof(countermoves)); // clear countermoves
    }

    inline void position(vector<string> &tokens)
    {
        int movesTokenIndex = -1;

        if (tokens[1] == "startpos")
        {
            board = Board(START_FEN);
            movesTokenIndex = 2;
        }
        else if (tokens[1] == "fen")
        {
            string fen = "";
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

    inline void go(vector<string> &tokens)
    {
        setupTime(tokens, board.sideToMove());
        Move bestMove = iterativeDeepening();
        cout << "bestmove " + bestMove.toUci() + "\n";
    }

    inline void info(int depth, i16 score)
    {
        auto millisecondsPassed = millisecondsElapsed(start);
        u64 nps = nodes / (millisecondsPassed > 0 ? millisecondsPassed : 1) * 1000;

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
        string strPv = pvLines[0][0].toUci();
        for (int i = 1; i < pvLengths[0]; i++)
            strPv += " " + pvLines[0][i].toUci();

        cout << "info depth " << depth
            << " seldepth " << maxPlyReached
            << " time " << round(millisecondsPassed)
            << " nodes " << nodes
            << " nps " << nps
            << (isMate ? " score mate " : " score cp ") << (isMate ? movesToMate : score)
            << " pv " << strPv
            << endl;
    }

    inline void uciLoop()
    {
        string received = "";
        while (received != "uci")
        {
            getline(cin, received);
            trim(received);
        }

        cout << "id name z5\n";
        cout << "id author zzzzz\n";
        cout << "option name Hash type spin default " << TT_DEFAULT_SIZE_MB << " min 1 max 1024\n";
        cout << "uciok\n";

        while (true)
        {
            getline(cin, received);
            trim(received);
            vector<string> tokens = splitString(received, ' ');
            if (received == "" || tokens.size() == 0)
                continue;

            try {

            if (received == "quit" || !cin.good())
                break;
            else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
                setoption(tokens);
            else if (received == "ucinewgame")
                ucinewgame();
            else if (received == "isready")
                cout << "readyok\n";
            else if (tokens[0] == "position")
                position(tokens);
            else if (tokens[0] == "go")
                go(tokens);
            else if (tokens[0] == "perft")
            {
                int depth = stoi(tokens[1]);
                perftBench(board, depth);
            }
            else if (tokens[0] == "eval")
                cout << "eval " << nnue::evaluate(board.sideToMove()) << " cp" << endl;

            } 
            catch (const char* errorMessage)
            {

            }
            
        }
    }

}


