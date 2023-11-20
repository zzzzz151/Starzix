#pragma once

// clang-format off

struct HistoryEntry
{
    i32 mainHistory = 0;
    i32 countermoveHistory[6][64];  // [lastMovePieceType][lastMoveTargetSquare]
    i32 followupMoveHistory[6][64]; // [lastLastMovePieceType][lastLastMoveTargetSquare]
    i32 captureHistory = 0;

    inline i32 quietHistory(Board &board)
    {
        // add main history
        i32 totalHist = mainHistory;
        
        // add countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != MOVE_NONE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(1);
            int targetSq = (int)lastMove.to();
            totalHist += countermoveHistory[pieceType][targetSq];
        }

        // add follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != MOVE_NONE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(2);
            int targetSq = (int)lastLastMove.to();
            totalHist += followupMoveHistory[pieceType][targetSq];
        }
       
        return totalHist;
    }

    inline void updateQuietHistory(Board &board, i32 bonus, i32 maxHistory)
    {
        // Update main history
        mainHistory += bonus - mainHistory * abs(bonus) / maxHistory;

        // Update countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != MOVE_NONE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(1);
            int targetSq = (int)lastMove.to();
            i32 countermoveHistDelta = bonus - countermoveHistory[pieceType][targetSq] * abs(bonus) / maxHistory;
            countermoveHistory[pieceType][targetSq] += countermoveHistDelta;
        }

        // Update follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != MOVE_NONE)
        {
            int pieceType = (int)board.getNthToLastMovePieceType(2);
            int targetSq = (int)lastLastMove.to();
            i32 followupHistDelta = bonus - followupMoveHistory[pieceType][targetSq] * abs(bonus) / maxHistory;
            followupMoveHistory[pieceType][targetSq] += followupHistDelta;
        }
    }

    inline void updateCaptureHistory(Board &board, i32 bonus, i32 maxHistory)
    {
        captureHistory += bonus - captureHistory * abs(bonus) / maxHistory;
    }

};