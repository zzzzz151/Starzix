#pragma once

// clang-format off

struct HistoryEntry
{
    int32_t mainHistory = 0;
    int32_t countermoveHistory[6][64];  // [lastMovePieceType][lastMoveTargetSquare]
    int32_t followupMoveHistory[6][64]; // [lastLastMovePieceType][lastLastMoveTargetSquare]

    inline int32_t totalHistory()
    {
        // add main history
        int32_t totalHist = mainHistory;
        
        // add countermove history
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != NULL_MOVE)
        {
            int lastMovePieceType = (int)board.getNthToLastMovePieceType(1);
            int lastMoveTargetSq = (int)lastMove.to();
            totalHist += countermoveHistory[lastMovePieceType][lastMoveTargetSq];
        }

        // add follow-up move history
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != NULL_MOVE)
        {
            int lastLastMovePieceType = (int)board.getNthToLastMovePieceType(2);
            int lastLastMoveTargetSq = (int)lastLastMove.to();
            totalHist += followupMoveHistory[lastLastMovePieceType][lastLastMoveTargetSq];
        }
       
        return totalHist;
    }

    inline void updateHistory(int32_t bonus)
    {
        // Update main history
        mainHistory += bonus - mainHistory * abs(bonus) / HISTORY_MAX;

        // Update countermove history
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != NULL_MOVE)
        {
            int lastMovePieceType = (int)board.getNthToLastMovePieceType(1);
            int lastMoveTargetSq = (int)lastMove.to();
            int32_t countermoveHistDelta = bonus - countermoveHistory[lastMovePieceType][lastMoveTargetSq] * abs(bonus) / HISTORY_MAX;
            countermoveHistory[lastMovePieceType][lastMoveTargetSq] += countermoveHistDelta;
        }

        // Update follow-up move history
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != NULL_MOVE)
        {
            int lastLastMovePieceType = (int)board.getNthToLastMovePieceType(2);
            int lastLastMoveTargetSq = (int)lastLastMove.to();
            int32_t followupHistDelta = bonus - followupMoveHistory[lastLastMovePieceType][lastLastMoveTargetSq] * abs(bonus) / HISTORY_MAX;
            followupMoveHistory[lastLastMovePieceType][lastLastMoveTargetSq] += followupHistDelta;
        }
    }

};