// g++  -std=c++2a main.cpp -o main

#include "chess.hpp"
#include <iostream>
#include <cstdlib> // For rand() function
#include <ctime>   // For seeding rand() with current time
#include <string>

using namespace std;
using namespace chess;

Board board;
Color color, enemyColor;

void position(vector<string> words)
{
    color = Color::WHITE;
    enemyColor = Color::BLACK;

    int movesTokenIndex = -1;
    if (words[1] == "startpos")
    {
        board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
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
    {
        // cout << "making move " + words[i] << endl;
        board.makeMove(uci::uciToMove(board, words[i]));
        Color temp = color;
        color = enemyColor;
        enemyColor = temp;
    }
}

void uciLoop()
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
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
            position(words);
        else if (words[0] == "go")
        {
            Movelist moves;
            movegen::legalmoves(moves, board);
            int randomIndex = rand() % moves.size();
            board.makeMove(moves[randomIndex]);
            cout << "bestmove " + uci::moveToUci(moves[randomIndex]) + "\n";
        }
    }
}

int main()
{
    uciLoop();
    return 0;
}
