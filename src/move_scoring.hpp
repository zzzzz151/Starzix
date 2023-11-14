#pragma once

// clang-format off

const int32_t PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

const int32_t TT_MOVE_SCORE = MAX_INT32,
              GOOD_CAPTURE_BASE_SCORE = 1'500'000'000,
              PROMOTION_BASE_SCORE = 1'000'000'000,
              KILLER_SCORE = 500'000'000,
              COUNTERMOVE_SCORE = 250'000'000,
              HISTORY_MOVE_BASE_SCORE = 0,
              BAD_CAPTURE_BASE_SCORE = -500'000'000;

inline int32_t MVVLVA(Move move)
{
    PieceType captured = board.captured(move);
    PieceType capturing = board.pieceTypeAt(move.from());
    return 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing];
}

inline array<int32_t, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove, bool qsearchCapturesOnly = false)
{
    array<int32_t, 256> movesScores;

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        if (move == ttMove)
            movesScores[i] = TT_MOVE_SCORE;
        else if (qsearchCapturesOnly)
            movesScores[i] = MVVLVA(move);
        else if (board.isCapture(move))
        {
            movesScores[i] = SEE(board, move) ? GOOD_CAPTURE_BASE_SCORE : BAD_CAPTURE_BASE_SCORE;
            movesScores[i] += MVVLVA(move);
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            movesScores[i] = PROMOTION_BASE_SCORE + move.typeFlag();
        else if (move == killerMove)
            movesScores[i] = KILLER_SCORE;
        else if (board.getLastMove() != NULL_MOVE && move == countermoves[board.sideToMove()][board.getLastMove().getMove()])
            movesScores[i] = COUNTERMOVE_SCORE;
        else
        {
            Color stm = board.sideToMove();
            int pieceType = (int)board.pieceTypeAt(move.from());
            int targetSquare = (int)move.to();
            movesScores[i] = HISTORY_MOVE_BASE_SCORE + historyTable[stm][pieceType][targetSquare].totalHistory(board);
        }
    }

    return movesScores;
}