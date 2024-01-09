#pragma once

// clang-format off

struct HistoryEntry
{
    i32 mainHistory;
    i32 continuationHistories[2][6][64]; // [ply-1][pieceType][targetSquare]
    i32 noisyHistory;

    inline HistoryEntry() {
        mainHistory = 0;
        memset(continuationHistories, 0, sizeof(continuationHistories));
        noisyHistory = 0;
    }

    inline i32 quietHistory(Board &board)
    {
        // add main history
        i32 quietHist = mainHistory;

        // add continuation histories
        Move move;
        for (u16 ply : {1, 2})
        {
            if ((move = board.getNthToLastMove(ply)) != MOVE_NONE)
            {
                u8 pt = (u8)move.pieceType();
                quietHist += continuationHistories[ply-1][pt][move.to()];
            }
        }
       
        return quietHist;
    }

    inline void updateQuietHistory(Board &board, i32 bonus)
    {
        // Update main history
        mainHistory += bonus - abs(bonus) * mainHistory / historyMax.value;

        // Update continuation histories
        Move move;
        for (u16 ply : {1, 2})
        {
            if ((move = board.getNthToLastMove(ply)) != MOVE_NONE)
            {
                u8 pt = (u8)move.pieceType();
                i32 *history = &continuationHistories[ply-1][pt][move.to()];
                *history += bonus - abs(bonus) * *history / historyMax.value;
            }
        }
    }

    inline void updateNoisyHistory(Board &board, i32 bonus) {
        noisyHistory += bonus - abs(bonus) * noisyHistory / historyMax.value;
    }

};