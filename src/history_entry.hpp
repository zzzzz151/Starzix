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
    MultiArray<i16, 2, 2>  mMainHist  = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist  = {}; // [previousMovePieceType][previousMoveTo]
    std::array<i16, 7>     mNoisyHist = {}; // [pieceTypeCaptured]

    public:

    inline i32 quietHistory(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves) 
    {
        const std::array<float, 3> CONT_HISTORY_WEIGHTS = { 
            contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight() 
        };

        float total = float(mMainHist[enemyAttacksOrigin][enemyAttacksDst]) * mainHistoryWeight();

        Move move;
        for (size_t i = 0; i < moves.size(); i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                int pt = (int)move.pieceType();
                total += float(mContHist[pt][move.to()]) * CONT_HISTORY_WEIGHTS[i];
            }

        return total;
    }

    inline void updateQuietHistories(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves, i32 bonus)
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
