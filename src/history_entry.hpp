// clang-format off

#pragma once

struct HistoryEntry {
    private:

    i16 mainHistory = 0;

    // [0 = 1ply, 1 = 2ply, 2 = 4ply][pieceType][targetSquare]
    MultiArray<i16, 3, 6, 64> continuationHistories = { };

    std::array<i16, 7> noisyHist = { }; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(Board &board)
    {
        // Add main history
        float total = (float)mainHistory * mainHistoryWeight();

        // Add continuation histories

        std::array<Move, 3> moves = { board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4) };
        std::array<float, 3> weights = { onePlyContHistWeight(), twoPlyContHistWeight(), fourPlyContHistWeight() };

        for (int i = 0; i < 3; i++) 
            if (moves[i] != MOVE_NONE) 
            {
                int pt = (int)moves[i].pieceType();
                Square to = moves[i].to();
                total += (float)continuationHistories[i][pt][to] * weights[i];
            }
       
        return total;
    }

    inline i32 noisyHistory(PieceType captured) {
        return noisyHist[(int)captured];
    }

    inline void updateQuietHistory(i32 bonus, Board &board)
    {
        // Update main history
        mainHistory += bonus - abs(bonus) * (i32)mainHistory / historyMax();

        // Update continuation histories

        std::array<Move, 3> moves = { board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4) };

        for (int i = 0; i < 3; i++)
            if (moves[i] != MOVE_NONE) 
            {
                int pt = (int)moves[i].pieceType();
                auto &history = continuationHistories[i][pt][moves[i].to()];
                history += bonus - abs(bonus) * (i32)history / historyMax();
            }
    }

    inline void updateNoisyHistory(i32 bonus, PieceType captured) 
    {
        noisyHist[(int)captured] += bonus - abs(bonus) * (i32)noisyHist[(int)captured] / historyMax();
    }

}; // struct HistoryEntry