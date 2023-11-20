#pragma once

#include <chrono>
#include "board.hpp"

inline u64 perft(Board &board, int depth)
{
    if (depth == 0) 
        return 1;

    MovesList moves = board.pseudolegalMoves();
    u64 nodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i]))
            continue;
        nodes += perft(board, depth - 1);
        board.undoMove();
    }

    return nodes;
}

inline void perftBench(Board &board, int depth)
{
    bool perftWasEnabled = board.perft;
    board.perft = true;

    chrono::steady_clock::time_point start =  chrono::steady_clock::now();
    u64 nodes = perft(board, depth);
    double millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
    u64 nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1.0) * 1000.0;
    cout << "perft depth " << depth << " time " << millisecondsElapsed << " nodes " << nodes << " nps " << nps << " fen " << board.fen() << endl;

    board.perft = perftWasEnabled;
}


inline void perftDivide(Board &board, int depth)
{
    MovesList moves = board.pseudolegalMoves(); 

    if (moves.size() == 0)
         return;

    u64 totalNodes = 0;
    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i])) 
            continue;
        u64 nodes = perft(board, depth - 1);
        cout << moves[i].toUci() << ": " << nodes << endl;
        totalNodes += nodes;
        board.undoMove();
    }

    cout << "Total: " << totalNodes << endl;
}

inline u64 perftCaptures(Board &board, int depth)
{
    if (depth == 0) 
        return 0;

    u64 legalCaptures = 0;
    if (depth == 1)
    {
        MovesList moves = board.pseudolegalMoves(true); // captures only
        for (int i = 0; i < moves.size(); i++) 
        {
            if (!board.makeMove(moves[i]))
                continue;
            legalCaptures++;
            board.undoMove();
        }
        return legalCaptures;
    }

    MovesList moves = board.pseudolegalMoves();        
    u64 nodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i]))
            continue;
        nodes += perftCaptures(board, depth - 1);
        board.undoMove();
    }

    return nodes;
}
