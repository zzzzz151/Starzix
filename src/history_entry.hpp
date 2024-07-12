// clang-format off

#pragma once

inline void updateQuietHistory(i16 &history, i32 bonus) {
    history = std::clamp((i32)history + bonus, -historyMax(), historyMax());
}

inline void updateNoisyHistory(i16* history, i32 bonus) {
    *history += bonus - abs(bonus) * i32(*history) / historyMax();
}

struct HistoryEntry {
    private:

    MultiArray<i16, 2, 2>  mMainHist  = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist  = {}; // [previousMovePieceType][previousMoveTo]
    MultiArray<i16, 7, 2>  mNoisyHist = {}; // [pieceTypeCaptured][enemyAttacksDst]

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
        updateQuietHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus);

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                updateQuietHistory(mContHist[pt][move.to()], bonus);
            }
    }

    inline i32 noisyHistory(PieceType captured, bool enemyAttacksDst) {
        return mNoisyHist[(int)captured][enemyAttacksDst];
    }

    inline i16* noisyHistoryPtr(PieceType captured, bool enemyAttacksDst) {
        return &mNoisyHist[(int)captured][enemyAttacksDst];
    }

}; // HistoryEntry
