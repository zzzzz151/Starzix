// clang-format off

#pragma once

struct HistoryEntry {
    public:

    i32 mainHistory = 0, noisyHistory = 0;

    // [ply-1][pieceType][targetSquare]
    std::array<std::array<std::array<i32, 64>, 6>, 2> continuationHistories = { };

    inline i32 quietHistory(Board &board)
    {
        // Add main history
        i32 total = mainHistory;

        // Add continuation histories
        Move move;
        for (int ply : {1, 2}) {
            if ((move = board.nthToLastMove(ply)) != MOVE_NONE)
            {
                int pt = (int)move.pieceType();
                total += continuationHistories[ply-1][pt][move.to()];
            }
        }
       
        return total;
    }

    inline void updateQuietHistory(Board &board, i32 bonus)
    {
        // Update main history
        mainHistory += bonus - abs(bonus) * mainHistory / historyMax.value;

        // Update continuation histories
        Move move;
        for (int ply : {1, 2}) {
            if ((move = board.nthToLastMove(ply)) != MOVE_NONE)
            {
                int pt = (int)move.pieceType();
                i32 &history = continuationHistories[ply-1][pt][move.to()];
                history += bonus - abs(bonus) * history / historyMax.value;
            }
        }
    }

    inline void updateNoisyHistory(i32 bonus) {
        noisyHistory += bonus - abs(bonus) * noisyHistory / historyMax.value;
    }

}; // struct HistoryEntry