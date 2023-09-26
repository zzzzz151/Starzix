#ifndef MOVE_SCORING_HPP
#define MOVE_SCORING_HPP

// clang-format off
#include <algorithm>
#include <iostream>
#include "search.hpp"
using namespace std;

const int HASH_MOVE_SCORE = INT_MAX,
          GOOD_CAPTURE_SCORE = 1'500'000'000,
          PROMOTION_SCORE = 1'000'000'000,
          KILLER_SCORE = 500'000'000,
          HISTORY_SCORE = 0, // non-killer quiets
          BAD_CAPTURE_SCORE = -500'000'000;

inline void scoreMoves(MovesList &moves, int *scores, uint64_t boardKey, TTEntry &ttEntry, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (ttEntry.key == boardKey && move == ttEntry.bestMove)
            scores[i] = HASH_MOVE_SCORE;
        else if (board.isCapture(move))
        {
            PieceType captured = board.pieceTypeAt(move.to());
            PieceType capturing = board.pieceTypeAt(move.from());
            int moveScore = SEE(board, move) ? GOOD_CAPTURE_SCORE : BAD_CAPTURE_SCORE;
            moveScore += 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing]; // MVVLVA
            scores[i] = moveScore;
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            scores[i] = PROMOTION_SCORE;
        else if (killerMoves[plyFromRoot][0] == move)
            scores[i] = KILLER_SCORE + 1;
        else if (killerMoves[plyFromRoot][1] == move)
            scores[i] = KILLER_SCORE;
        else if (move == counterMove)
            scores[i] = KILLER_SCORE - 1;
        else
        {
            int stm = (int)board.colorToMove();
            int pieceType = (int)board.pieceTypeAt(move.from());
            int squareTo = (int)move.to();
            scores[i] = HISTORY_SCORE + historyMoves[stm][pieceType][squareTo];
        }
    }
}

inline Move incrementalSort(MovesList &moves, int *scores, int i)
{
    for (int j = i + 1; j < moves.size(); j++)
        if (scores[j] > scores[i])
        {
            moves.swap(i, j);
            swap(scores[i], scores[j]);
        }

    return moves[i];
}

#endif