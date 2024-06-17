// clang-format off

#pragma once

std::array<float, 3> CONT_HISTS_WEIGHTS = { onePlyContHistWeight(), twoPlyContHistWeight(), fourPlyContHistWeight() };

struct HistoryEntry {
    private:

    i16 mMainHistory = 0;

    // Continuation histories 
    // [0 = 1ply, 1 = 2ply, 2 = 4ply][pieceType][targetSquare]
    MultiArray<i16, 3, 6, 64> mContHists = { };

    std::array<i16, 7> mNoisyHistory = { }; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(std::array<Move, 3> moves)
    {
        // Add main history
        float total = (float)mMainHistory * mainHistoryWeight();

        // Add continuation histories

        Move move;
        for (std::size_t i = 0; i < 3; i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                int pt = (int)move.pieceType();
                total += (float)mContHists[i][pt][move.to()] * CONT_HISTS_WEIGHTS[i];
            }
       
        return total;
    }

    inline i32 noisyHistory(PieceType captured) {
        return mNoisyHistory[(int)captured];
    }

    inline void updateQuietHistory(i32 bonus, std::array<Move, 3> moves)
    {
        // Update main history
        mMainHistory += bonus - abs(bonus) * (i32)mMainHistory / historyMax();

        // Update continuation histories

        Move move;
        for (std::size_t i = 0; i < 3; i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                int pt = (int)move.pieceType();
                auto &history = mContHists[i][pt][move.to()];
                history += bonus - abs(bonus) * (i32)history / historyMax();
            }
    }

    inline void updateNoisyHistory(i32 bonus, PieceType captured) 
    {
        mNoisyHistory[(int)captured] += bonus - abs(bonus) * (i32)mNoisyHistory[(int)captured] / historyMax();
    }

}; // struct HistoryEntry