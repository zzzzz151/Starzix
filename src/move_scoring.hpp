#pragma once

// clang-format off

const int HASH_MOVE_SCORE = INT_MAX,
          GOOD_CAPTURE_SCORE = 1'500'000'000,
          PROMOTION_SCORE = 1'000'000'000,
          KILLER_SCORE = 500'000'000,
          COUNTERMOVE_SCORE = 250'000'000,
          HISTORY_SCORE = 0, // non-killer quiets
          BAD_CAPTURE_SCORE = -500'000'000;

inline void scoreMoves(MovesList &moves, int *scores, TTEntry &ttEntry, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (board.zobristHash() == ttEntry.zobristHash && move == ttEntry.bestMove)
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
            scores[i] = PROMOTION_SCORE + move.typeFlag();
        else if (killerMoves[plyFromRoot][0] == move)
            scores[i] = KILLER_SCORE + 1;
        else if (killerMoves[plyFromRoot][1] == move)
            scores[i] = KILLER_SCORE;
        else if (move == counterMoves[board.enemyColor()][board.lastMove().move()])
            scores[i] = COUNTERMOVE_SCORE;
        else
        {
            int stm = (int)board.colorToMove();
            int pieceType = (int)board.pieceTypeAt(move.from());
            int targetSqiare = (int)move.to();
            scores[i] = HISTORY_SCORE + history[stm][pieceType][targetSqiare];
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

