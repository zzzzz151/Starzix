// clang-format off

#pragma once

#include "board.hpp"

inline u64 perft(Board &board, const int depth)
{
    if (depth <= 0) return 1;

    ArrayVec<Move, 256> moves;
    board.pseudolegalMoves(moves, MoveGenType::ALL);

    u64 nodes = 0;

    if (depth == 1) {
         for (const Move move : moves)
            nodes += board.isPseudolegalLegal(move);

        return nodes;
    }

    for (const Move move : moves) 
        if (board.isPseudolegalLegal(move))
        {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.undoMove();
        }

    return nodes;
}

inline void perftSplit(Board &board, const int depth)
{
    if (depth <= 0) return;

    std::cout << "Running split perft depth " << depth 
              << " on " << board.fen() 
              << std::endl;

    ArrayVec<Move, 256> moves;
    board.pseudolegalMoves(moves, MoveGenType::ALL);

    u64 totalNodes = 0;

    if (depth == 1) {
        for (const Move move : moves)
            if (board.isPseudolegalLegal(move))
                std::cout << move.toUci() << ": 1" << std::endl;   

        return;
    }

    for (const Move move : moves) 
        if (board.isPseudolegalLegal(move))
        {
            board.makeMove(move);
            const u64 nodes = perft(board, depth - 1);
            std::cout << move.toUci() << ": " << nodes << std::endl;
            totalNodes += nodes;
            board.undoMove();
        }

    std::cout << "Total: " << totalNodes << std::endl;
}

inline u64 perftBench(Board &board, int depth)
{
    depth = std::max(depth, 0);
    const std::string fen = board.fen();

    std::cout << "Running perft depth " << depth << " on " << fen << std::endl;

    const std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
    const u64 nodes = perft(board, depth);

    std::cout << "perft depth " << depth 
              << " nodes " << nodes 
              << " nps " << nodes * 1000 / std::max((u64)millisecondsElapsed(start), (u64)1)
              << " time " << millisecondsElapsed(start)
              << " fen " << fen
              << std::endl;

    return nodes;
}
