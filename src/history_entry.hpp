// clang-format off

#pragma once

struct HistoryEntry {
    public:

    i32 mainHistory = 0, noisyHistory = 0;

    // [0 = 1ply, 1 = 2ply, 2 = 4ply][pieceType][targetSquare]
    std::array<std::array<std::array<i32, 64>, 6>, 3> continuationHistories = { };

    inline i32 quietHistory(Board &board)
    {
        // Add main history
        i32 total = mainHistory;

        Move move;

        // Add continuation history 1 ply
        if ((move = board.nthToLastMove(1)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            total += continuationHistories[0][pt][move.to()];
        }

        // Add continuation history 2 ply
        if ((move = board.nthToLastMove(2)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            total += continuationHistories[1][pt][move.to()];
        }

        // Add continuation history 4 ply
        if ((move = board.nthToLastMove(4)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            total += continuationHistories[2][pt][move.to()];
        }
       
        return total;
    }

    inline void updateQuietHistory(Board &board, i32 bonus)
    {
        // Update main history
        i32 thisBonus = bonus * historyBonusMultiplierMain.value;
        mainHistory += thisBonus - abs(thisBonus) * mainHistory / historyMax.value;

        Move move;

        // Update continuation history 1 ply
        if ((move = board.nthToLastMove(1)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            i32 &history = continuationHistories[0][pt][move.to()];
            thisBonus = bonus * historyBonusMultiplier1Ply.value;
            history += thisBonus - abs(thisBonus) * history / historyMax.value;
        }

        // Update continuation history 2 ply
        if ((move = board.nthToLastMove(2)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            i32 &history = continuationHistories[1][pt][move.to()];
            thisBonus = bonus * historyBonusMultiplier2Ply.value;
            history += thisBonus - abs(thisBonus) * history / historyMax.value;
        }

        // Update continuation history 4 ply
        if ((move = board.nthToLastMove(4)) != MOVE_NONE)
        {
            int pt = (int)move.pieceType();
            i32 &history = continuationHistories[2][pt][move.to()];
            thisBonus = bonus * historyBonusMultiplier4Ply.value;
            history += thisBonus - abs(thisBonus) * history / historyMax.value;
        }
    }

    inline void updateNoisyHistory(i32 bonus) {
        bonus *= historyBonusMultiplierNoisy.value;
        noisyHistory += bonus - abs(bonus) * noisyHistory / historyMax.value;
    }

}; // struct HistoryEntry