#include <iostream>
#include <string>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>   // For rand() and srand()
#include <ctime>     // For time()
#include "../src-test/board/board.hpp"
using namespace std;

Board board;

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

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();

    string received;
    getline(cin, received);
    cout << "id name random_mover\n";
    cout << "id author zzzzz\n";
    cout << "uciok\n";
    srand(static_cast<unsigned int>(time(nullptr))); // Seed the random number generator with the current time

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
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
            position(words);
        else if (words[0] == "go")
        {
            MovesList moves = board.pseudolegalMoves();
            if (moves.size() == 0) continue;

            cerr << board.fen() << endl;
            cerr << "Pseudolegals: ";
            for (int i = 0; i < moves.size(); i++)
                cerr << moves[i].toUci() << " ";
            cerr << endl;

            int maxIndex = moves.size() - 1;
            vector<int> indexes;

            // Fill vector with indexes from 0 to maxIndex
            for (int i = 0; i <= maxIndex; i++) 
                indexes.push_back(i); 

            // Shuffle the vector
            for (int i = 0; i <= maxIndex; i++) {
                int randomIndex = rand() % maxIndex;
                swap(indexes[i], indexes[randomIndex]);
            }

            Move move;
            for (int idx : indexes)
            {
                cerr << "Testing " << moves[idx].toUci() << ", ";
                if (board.makeMove(moves[idx]))
                {
                    cerr << endl << moves[idx].toUci() << " is legal" << endl;
                    // this move is legal
                    move = moves[idx];
                    board.undoMove();
                    break;
                }
            }
            cerr << "besmov " << move.toUci() << endl;
            cout << "bestmove " << move.toUci() + "\n";

        }
    }
}