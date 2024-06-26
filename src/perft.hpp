// clang-format off

#pragma once

#include "board.hpp"

inline u64 perft(Board &board, int depth)
{
    if (depth <= 0) return 1;

    ArrayVec<Move, 256> moves;
    board.pseudolegalMoves(moves);
    u64 nodes = 0;
    u64 pinned = board.pinned();

    if (depth == 1) {
         for (Move move : moves)
            nodes += board.isPseudolegalLegal(move, pinned);

        return nodes;
    }

    for (Move move : moves)
        if (board.isPseudolegalLegal(move, pinned))
        {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.undoMove();
        }

    return nodes;
}

inline void perftSplit(Board &board, int depth)
{
    if (depth <= 0) return;

    std::cout << "Running split perft depth " << depth 
              << " on " << board.fen() 
              << std::endl;

    ArrayVec<Move, 256> moves;
    board.pseudolegalMoves(moves);
    u64 totalNodes = 0;
    u64 pinned = board.pinned();

    if (depth == 1) {
        for (Move move : moves)
            if (board.isPseudolegalLegal(move, pinned))
                std::cout << move.toUci() << ": 1" << std::endl;   

        return;
    }

    for (Move move : moves) 
        if (board.isPseudolegalLegal(move, pinned))
        {
            board.makeMove(move);
            u64 nodes = perft(board, depth - 1);
            std::cout << move.toUci() << ": " << nodes << std::endl;
            totalNodes += nodes;
            board.undoMove();
        }

    std::cout << "Total: " << totalNodes << std::endl;
}

inline u64 perftBench(Board &board, int depth)
{
    depth = std::max(depth, 0);
    std::string fen = board.fen();

    std::cout << "Running perft depth " << depth << " on " << fen << std::endl;

    std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    u64 nodes = perft(board, depth);

    std::cout << "perft depth " << depth 
              << " nodes " << nodes 
              << " nps " << nodes * 1000 / std::max((u64)millisecondsElapsed(start), (u64)1)
              << " time " << millisecondsElapsed(start)
              << " fen " << fen
              << std::endl;

    return nodes;
}
