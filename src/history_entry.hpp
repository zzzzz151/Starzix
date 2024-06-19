// clang-format off

#pragma once

std::array<float, 3> CONT_HISTS_WEIGHTS = { onePlyContHistWeight(), twoPlyContHistWeight(), fourPlyContHistWeight() };

inline void updateHistory(i16 &history, i32 bonus) {
    history += bonus - abs(bonus) * (i32)history / historyMax();
}

inline void updateHistory(i16* history, i32 bonus) {
    updateHistory(*history, bonus);
}

struct HistoryEntry {
    private:

    MultiArray<i16, 2, 2> mMainHistory = { }; // [enemyAttacksOrigin][enemyAttacksDestination]

    // Continuation histories 
    // [0 = 1ply, 1 = 2ply, 2 = 4ply][pieceType][targetSquare]
    MultiArray<i16, 3, 6, 64> mContHists = { };

    std::array<i16, 7> mNoisyHistory = { }; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves)
    {
        // Add main history
        float total = (float)mMainHistory[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight();

        // Add continuation histories

        Move move;
        for (size_t i = 0; i < 3; i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                int pt = (int)move.pieceType();
                total += (float)mContHists[i][pt][move.to()] * CONT_HISTS_WEIGHTS[i];
            }
       
        return total;
    }

    inline void updateQuietHistory(i32 bonus, bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves)
    {
        // Update main history
        updateHistory(mMainHistory[enemyAttacksOrigin][enemyAttacksDst], bonus);

        // Update continuation histories

        Move move;
        for (size_t i = 0; i < 3; i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                int pt = (int)move.pieceType();
                updateHistory(mContHists[i][pt][move.to()], bonus);
            }
    }

    inline i32 noisyHistory(PieceType captured) {
        return mNoisyHistory[(int)captured];
    }

    inline i16* noisyHistoryPtr(PieceType captured) {
        return &mNoisyHistory[(int)captured];
    }

    inline void updateNoisyHistory(i32 bonus, PieceType captured) {
        updateHistory(mNoisyHistory[(int)captured], bonus);
    }

}; // struct HistoryEntry