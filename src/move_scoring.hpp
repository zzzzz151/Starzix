#pragma once

// clang-format off

const int32_t TT_MOVE_SCORE = MAX_INT32,
              GOOD_CAPTURE_BASE_SCORE = 1'500'000'000,
              PROMOTION_BASE_SCORE = 1'000'000'000,
              KILLER_SCORE = 500'000'000,
              COUNTERMOVE_SCORE = 250'000'000,
              HISTORY_MOVE_BASE_SCORE = 0,
              BAD_CAPTURE_BASE_SCORE = -500'000'000;

// Most valuable victim        P    N    B    R    Q
const int32_t MVV_VALUES[5] = {100, 300, 320, 500, 900};

inline array<int32_t, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove, bool doSEE = true)
{
    array<int32_t, 256> movesScores;
    Color stm = board.sideToMove();

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();
        PieceType pieceTypeCaptured = board.captured(move);

        if (move == ttMove)
            movesScores[i] = TT_MOVE_SCORE;
        else if (pieceTypeCaptured != PieceType::NONE)
        {
            movesScores[i] = MVV_VALUES[(int)pieceTypeCaptured];
            movesScores[i] += historyTable[stm][pieceType][targetSquare].captureHistory;
            if (doSEE) movesScores[i] += SEE(board, move) ? GOOD_CAPTURE_BASE_SCORE : BAD_CAPTURE_BASE_SCORE;
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            movesScores[i] = PROMOTION_BASE_SCORE + move.typeFlag();
        else if (move == killerMove)
            movesScores[i] = KILLER_SCORE;
        else if (board.getLastMove() != NULL_MOVE && move == countermoves[stm][board.getLastMove().getMove()])
            movesScores[i] = COUNTERMOVE_SCORE;
        else
        {
            movesScores[i] = HISTORY_MOVE_BASE_SCORE;
            movesScores[i] += historyTable[stm][pieceType][targetSquare].quietHistory(board);
        }
    }

    return movesScores;
}