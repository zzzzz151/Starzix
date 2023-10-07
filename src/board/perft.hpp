#pragma once

#include "board.hpp"

inline uint64_t perft(Board &board, int depth)
{
    if (depth == 0) return 1;

    MovesList moves = board.pseudolegalMoves();
    uint64_t nodes = 0;

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
    chrono::steady_clock::time_point start =  chrono::steady_clock::now();
    uint64_t nodes = perft(board, depth);
    double millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
    uint64_t nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1.0) * 1000.0;
    cout << "perft depth " << depth << " time " << millisecondsElapsed << " nodes " << nodes << " nps " << nps << " fen " << board.fen() << endl;
}