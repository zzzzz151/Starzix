// clang-format off

#pragma once

struct HistoryEntry {
    private:

    i16 mMainHistory = 0;

    // [0 = 1ply, 1 = 2ply, 2 = 4ply][pieceType][targetSquare]
    MultiArray<i16, 3, 6, 64> mContHists = { };

    std::array<i16, 7> mNoisyHistory = { }; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(std::array<Move, 4> moves)
    {
        // Add main history
        float total = (float)mMainHistory * mainHistoryWeight();

        // Add continuation histories

        std::array<float, 3> weights = { onePlyContHistWeight(), twoPlyContHistWeight(), fourPlyContHistWeight() };

        for (std::size_t i = 1; i < moves.size(); i++)
            if (moves[i] != MOVE_NONE) 
            {
                PieceType pt = moves[i].pieceType();
                Square to = moves[i].to();
                total += (float)mContHists[i][(int)pt][to] * weights[i];
            }
       
        return total;
    }

    inline i32 noisyHistory(PieceType captured) {
        return mNoisyHistory[(int)captured];
    }

    inline void updateQuietHistory(i32 bonus, std::array<Move, 4> moves)
    {
        // Update main history
        mMainHistory += bonus - abs(bonus) * (i32)mMainHistory / historyMax();

        // Update continuation histories

        for (std::size_t i = 1; i < moves.size(); i++)
            if (moves[i] != MOVE_NONE) 
            {
                PieceType pt = moves[i].pieceType();
                Square to = moves[i].to();

                auto &history = mContHists[i-1][(int)pt][to];
                history += bonus - abs(bonus) * (i32)history / historyMax();
            }
    }

    inline void updateNoisyHistory(i32 bonus, PieceType captured) 
    {
        mNoisyHistory[(int)captured] += bonus - abs(bonus) * (i32)mNoisyHistory[(int)captured] / historyMax();
    }

}; // struct HistoryEntry