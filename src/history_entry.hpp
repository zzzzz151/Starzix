// clang-format off

#pragma once

inline void updateHistory(i16 &history, i32 bonus) {
    history = std::clamp((i32)history + bonus, -historyMax(), historyMax());
}

inline void updateHistory(i16* history, i32 bonus) {
    updateHistory(*history, bonus);
}

struct HistoryEntry {
    private:

    MultiArray<i16, 2, 2>  mMainHist  = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist  = {}; // [previousMovePieceType][previousMoveTo]
    std::array<i16, 7>     mNoisyHist = {}; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 2> moves) 
    {
        float total = mMainHist[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight();

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                total += mContHist[pt][move.to()] * contHistoryWeight();
            }

        return total;
    }

    inline void updateQuietHistories(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 2> moves, i32 bonus)
    {
        updateHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus);

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                updateHistory(mContHist[pt][move.to()], bonus);
            }
    }

    inline i32 noisyHistory(PieceType captured) {
        return mNoisyHist[(int)captured];
    }

    inline i16* noisyHistoryPtr(PieceType captured) {
        return &mNoisyHist[(int)captured];
    }

}; // HistoryEntry
