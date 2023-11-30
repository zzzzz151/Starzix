#pragma once

#include <chrono>
#include "board.hpp"

namespace perft
{

inline u64 perft(Board &board, int depth)
{
    if (depth == 0) return 1;

    MovesList moves = board.pseudolegalMoves();
    u64 nodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (board.makeMove(moves[i]))
        {
            nodes += perft(board, depth - 1);
            board.undoMove();
        }
    }

    return nodes;
}

inline void perftSplit(Board &board, int depth)
{
    std::string fen = board.fen();
    board = Board(fen, true); // second arg = true => don't update zobrist hash nor NNUE in perft

    MovesList moves = board.pseudolegalMoves(); 
    u64 totalNodes = 0;

    for (int i = 0; i < moves.size(); i++) 
    {
        if (board.makeMove(moves[i])) 
        {
            u64 nodes = perft(board, depth - 1);
            std::cout << moves[i].toUci() << ": " << nodes << std::endl;
            totalNodes += nodes;
            board.undoMove();
        }
    }

    std::cout << "Total: " << totalNodes << std::endl;

    // rebuild board to fix zobrist hash and NNUE
    board = Board(fen); 
}

inline u64 perftBench(Board &board, int depth)
{
    std::string fen = board.fen();
    board = Board(fen, true); // second arg = true => don't update zobrist hash nor NNUE in perft

    std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    u64 nodes = perft(board, depth);
    double millisecondsElapsed = (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
    u64 nps = nodes / (millisecondsElapsed > 0 ? millisecondsElapsed : 1.0) * 1000.0;

    std::cout << "perft depth " << depth 
              << " nodes " << nodes 
              << " nps " << nps 
              << " time " << millisecondsElapsed 
              << " fen " << fen
              << std::endl;

    // rebuild board to fix zobrist hash and NNUE
    board = Board(fen); 

    return nodes;
}

}