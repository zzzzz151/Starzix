#pragma once

#include <chrono>
#include "board.hpp"

namespace perft
{

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

inline void perftSplit(Board &board, int depth)
{
    board.perft = true; // dont update zobrist hash nor NNUE

    MovesList moves = board.pseudolegalMoves(); 
    u64 totalNodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (!board.makeMove(moves[i])) 
            continue;
        u64 nodes = perft(board, depth - 1);
        std::cout << moves[i].toUci() << ": " << nodes << std::endl;
        totalNodes += nodes;
        board.undoMove();
    }

    std::cout << "Total: " << totalNodes << std::endl;

    // rebuild board to fix zobrist hash and NNUE
    board = Board(board.fen()); 
}

inline u64 perftBench(Board &board, int depth)
{
    board.perft = true; // dont update zobrist hash nor NNUE

    std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    u64 nodes = perft(board, depth);
    double millisecondsElapsed = (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
    u64 nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1.0) * 1000.0;

    std::cout << "perft depth " << depth 
              << " time " << millisecondsElapsed 
              << " nodes " << nodes 
              << " nps " << nps 
              << " fen " << board.fen() 
              << std::endl;

    // rebuild board to fix zobrist hash and NNUE
    board = Board(board.fen()); 

    return nodes;
}

}