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

        // Add continuation histories

        std::array<Move, 3> moves = { board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4) };

        for (int i = 0; i < 3; i++) 
            if (moves[i] != MOVE_NONE) {
                int pt = (int)moves[i].pieceType();
                total += continuationHistories[i][pt][moves[i].to()];
            }
       
        return total;
    }

    inline void updateQuietHistory(Board &board, i32 bonus)
    {
        // Update main history
        i32 thisBonus = bonus * historyBonusMultiplierMain.value;
        mainHistory += thisBonus - abs(thisBonus) * mainHistory / historyMax.value;

        // Update continuation histories

        std::array<Move, 3> moves = { board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4) };

        std::array<float, 3> bonusMultipliers = { historyBonusMultiplier1Ply.value, 
                                                  historyBonusMultiplier2Ply.value, 
                                                  historyBonusMultiplier4Ply.value };

        for (int i = 0; i < 3; i++)
            if (moves[i] != MOVE_NONE) {
                int pt = (int)moves[i].pieceType();
                auto &history = continuationHistories[i][pt][moves[i].to()];
                thisBonus = bonus * bonusMultipliers[i];
                history += thisBonus - abs(thisBonus) * history / historyMax.value;
            }
    }

    inline void updateNoisyHistory(i32 bonus) {
        bonus *= historyBonusMultiplierNoisy.value;
        noisyHistory += bonus - abs(bonus) * noisyHistory / historyMax.value;
    }

}; // struct HistoryEntry