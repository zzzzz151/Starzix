#pragma once

// clang-format off

struct HistoryEntry
{
    int32_t mainHistory = 0;
    int32_t countermoveHistory[6][64];  // [lastMovePieceType][lastMoveTargetSquare]
    int32_t followupMoveHistory[6][64]; // [lastLastMovePieceType][lastLastMoveTargetSquare]
    int32_t captureHistory = 0;

    inline int32_t quietHistory(Board &board)
    {
        // add main history
        int32_t totalHist = mainHistory;
        
        // add countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != NULL_MOVE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(1);
            int targetSq = (int)lastMove.to();
            totalHist += countermoveHistory[pieceType][targetSq];
        }

        // add follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != NULL_MOVE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(2);
            int targetSq = (int)lastLastMove.to();
            totalHist += followupMoveHistory[pieceType][targetSq];
        }
       
        return totalHist;
    }

    inline void updateQuietHistory(Board &board, int32_t bonus)
    {
        // Update main history
        mainHistory += bonus - mainHistory * abs(bonus) / HISTORY_MAX;

        // Update countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != NULL_MOVE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(1);
            int targetSq = (int)lastMove.to();
            int32_t countermoveHistDelta = bonus - countermoveHistory[pieceType][targetSq] * abs(bonus) / HISTORY_MAX;
            countermoveHistory[pieceType][targetSq] += countermoveHistDelta;
        }

        // Update follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != NULL_MOVE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(2);
            int targetSq = (int)lastLastMove.to();
            int32_t followupHistDelta = bonus - followupMoveHistory[pieceType][targetSq] * abs(bonus) / HISTORY_MAX;
            followupMoveHistory[pieceType][targetSq] += followupHistDelta;
        }
    }

    inline void updateCaptureHistory(Board &board, int32_t bonus)
    {
        captureHistory += bonus - captureHistory * abs(bonus) / HISTORY_MAX;
    }

};