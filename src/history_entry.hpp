// clang-format off

#pragma once

struct HistoryEntry {
    private:
    MultiArray<i16, 2, 2>  mMainHist = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist = {}; // [previousMovePieceType][previousMoveTo]

    public:

    inline i32 total(bool enemyAttacksOrigin, bool enemyAttacksDst, Move prevMove) 
    {
        if (prevMove == MOVE_NONE) 
            return mMainHist[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight();

        int pt = (int)prevMove.pieceType();

        return mMainHist[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight() 
             + mContHist[pt][prevMove.to()]                   * contHistoryWeight();
    }

    inline void update(i32 bonus, bool enemyAttacksOrigin, bool enemyAttacksDst, Move prevMove) 
    {
        mMainHist[enemyAttacksOrigin][enemyAttacksDst] 
            = std::clamp(mMainHist[enemyAttacksOrigin][enemyAttacksDst] + bonus, -historyMax(), historyMax());

        if (prevMove != MOVE_NONE) {
            int pt = (int)prevMove.pieceType();

            mContHist[pt][prevMove.to()] 
                = std::clamp(mContHist[pt][prevMove.to()] + bonus, -historyMax(), historyMax());
        }
    }

}; // HistoryEntry
