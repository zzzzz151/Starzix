#pragma once

// clang-format off

struct HistoryEntry
{
    i32 mainHistory;
    i32 countermoveHistory[6][64];  // [lastMovePieceType][lastMoveTargetSquare]
    i32 followupMoveHistory[6][64]; // [lastLastMovePieceType][lastLastMoveTargetSquare]
    i32 noisyHistory;

    inline HistoryEntry() {
        mainHistory = 0;
        memset(countermoveHistory, 0, sizeof(countermoveHistory));
        memset(followupMoveHistory, 0, sizeof(followupMoveHistory));
        noisyHistory = 0;
    }

    inline i32 quietHistory(Board &board)
    {
        // add main history
        i32 quietHist = mainHistory;
        
        // add countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != MOVE_NONE)
        {
            u8 pieceType = (u8)lastMove.pieceType();
            u8 targetSq = (u8)lastMove.to();
            quietHist += countermoveHistory[pieceType][targetSq];
        }

        // add follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != MOVE_NONE)
        {
            u8 pieceType = (u8)lastLastMove.pieceType();
            u8 targetSq = (u8)lastLastMove.to();
            quietHist += followupMoveHistory[pieceType][targetSq];
        }
       
        return quietHist;
    }

    inline void updateQuietHistory(Board &board, i32 bonus)
    {
        // Update main history
        mainHistory += bonus - abs(bonus) * mainHistory / historyMax.value;

        // Update countermove history using last move
        Move lastMove;
        if ((lastMove = board.getNthToLastMove(1)) != MOVE_NONE)
        {
            u8 pt = (u8)lastMove.pieceType();
            u8 targetSq = (u8)lastMove.to();

            countermoveHistory[pt][targetSq] 
                += bonus - abs(bonus) * countermoveHistory[pt][targetSq] / historyMax.value;
        }

        // Update follow-up move history using 2nd-to-last move
        Move lastLastMove;
        if ((lastLastMove = board.getNthToLastMove(2)) != MOVE_NONE)
        {
            u8 pt = (u8)lastLastMove.pieceType();
            u8 targetSq = (u8)lastLastMove.to();
            followupMoveHistory[pt][targetSq] 
                += bonus - abs(bonus) * followupMoveHistory[pt][targetSq] / historyMax.value;
        }
    }

    inline void updateNoisyHistory(Board &board, i32 bonus) {
        noisyHistory += bonus - abs(bonus) * noisyHistory / historyMax.value;
    }

};