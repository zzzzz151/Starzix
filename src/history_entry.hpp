// clang-format off

#pragma once

inline void updateHistory(i16* history, i32 bonus) 
{
    bonus = std::clamp(bonus, -HISTORY_MAX, HISTORY_MAX);
    *history += bonus - abs(bonus) * i32(*history) / HISTORY_MAX;
    assert(*history >= -HISTORY_MAX && *history <= HISTORY_MAX);
}

inline void updateHistory(i16 &history, const i32 bonus) {
    updateHistory(&history, bonus);
}

struct HistoryEntry {
    private:
    
    MultiArray<i16, 2, 2>  mMainHist  = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist  = {}; // [previousMovePieceType][previousMoveTo]
    std::array<i16, 7>     mNoisyHist = {}; // [pieceTypeCaptured]

    public:
    
    i16 mCorrHist = 0;

    inline i32 quietHistory(
        const bool enemyAttacksOrigin, const bool enemyAttacksDst, const std::array<Move, 3> moves) const
    {
        const std::array<float, 3> CONT_HISTORY_WEIGHTS = { 
            contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight() 
        };

        float total = float(mMainHist[enemyAttacksOrigin][enemyAttacksDst]) * mainHistoryWeight();

        Move move;
        for (size_t i = 0; i < moves.size(); i++)
            if ((move = moves[i]) != MOVE_NONE) 
            {
                const int pt = int(move.pieceType());
                total += float(mContHist[pt][move.to()]) * CONT_HISTORY_WEIGHTS[i];
            }

        return total;
    }

    inline void updateQuietHistories(
        const bool enemyAttacksOrigin, const bool enemyAttacksDst, const std::array<Move, 3> moves, const i32 bonus)
    {
        updateHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus);

        for (const Move move : moves) 
            if (move != MOVE_NONE) {
                const int pt = int(move.pieceType());
                updateHistory(mContHist[pt][move.to()], bonus);
            }
    }

    inline i32 noisyHistory(const PieceType captured) const  {
        return mNoisyHist[(int)captured];
    }

    inline i16* noisyHistoryPtr(const PieceType captured) {
        return &mNoisyHist[(int)captured];
    }

}; // HistoryEntry
