// clang-format off

#pragma once

inline void updateHistory(i16* history, i32 bonus) {
    *history += bonus - abs(bonus) * i32(*history) / historyMax();
    assert(*history >= -historyMax() && *history <= historyMax());
}

inline void updateHistory(i16 &history, i32 bonus) {
    updateHistory(&history, bonus);
}

struct HistoryEntry {
    private:
    MultiArray<i16, 2, 2>  mMainHist = {};  // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist = {};  // [previousMovePieceType][previousMoveTo]
    std::array<i16, 7>     mNoisyHist = {}; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves) 
    {
        i32 total = mMainHist[enemyAttacksOrigin][enemyAttacksDst];

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                total += mContHist[pt][move.to()];
            }

        return total;
    }

    inline void updateQuietHistories(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves, i32 bonus)
    {
        const std::array<float, 3> CONT_HISTORY_WEIGHTS = { 
            contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight() 
        };

        updateHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus * mainHistoryWeight());

        for (size_t i = 0; i < 3; i++) 
        {
            Move move = moves[i];
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                updateHistory(mContHist[pt][move.to()], bonus * CONT_HISTORY_WEIGHTS[i]);
            }
        }
    }

    inline i32 noisyHistory(PieceType captured) {
        return mNoisyHist[(int)captured];
    }

    inline i16* noisyHistoryPtr(PieceType captured) {
        return &mNoisyHist[(int)captured];
    }

}; // HistoryEntry
