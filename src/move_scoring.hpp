#pragma once

// clang-format off

                              // P    N    B    R    Q    K      NONE
const int32_t PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

const int32_t TT_MOVE_SCORE = MAX_INT32,
              GOOD_CAPTURE_BASE_SCORE = 1'500'000'000,
              PROMOTION_BASE_SCORE = 1'000'000'000,
              KILLER_SCORE = 500'000'000,
              COUNTERMOVE_SCORE = 250'000'000,
              HISTORY_MOVE_BASE_SCORE = 0,
              BAD_CAPTURE_BASE_SCORE = -500'000'000;

inline array<int32_t, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove)
{
    array<int32_t, 256> scores;

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (move == ttMove)
            scores[i] = TT_MOVE_SCORE;
        else if (board.isCapture(move))
        {
            scores[i] = SEE(board, move) ? GOOD_CAPTURE_BASE_SCORE : BAD_CAPTURE_BASE_SCORE;
            PieceType captured = board.pieceTypeAt(move.to());
            PieceType capturing = board.pieceTypeAt(move.from());
            scores[i] += 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing]; // MVVLVA
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            scores[i] = PROMOTION_BASE_SCORE + move.typeFlag();
        else if (move == killerMove)
            scores[i] = KILLER_SCORE;
        else if (move == counterMoves[board.oppSide()][board.getLastMove().getMove()])
            scores[i] = COUNTERMOVE_SCORE;
        else
        {
            int stm = (int)board.sideToMove();
            int pieceType = (int)board.pieceTypeAt(move.from());
            int targetSquare = (int)move.to();
            scores[i] = HISTORY_MOVE_BASE_SCORE + historyTable[stm][pieceType][targetSquare].totalHistory();
        }
    }

    return scores;
}

inline pair<Move, int32_t> incrementalSort(MovesList &moves, array<int32_t, 256> &movesScores, int i)
{
    for (int j = i + 1; j < moves.size(); j++)
        if (movesScores[j] > movesScores[i])
        {
            moves.swap(i, j);
            swap(movesScores[i], movesScores[j]);
        }

    return { moves[i], movesScores[i] };
}

