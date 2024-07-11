// clang-format off

#pragma once

struct HistoryEntry {
    private:
    MultiArray<i16, 2, 2>  mMainHist = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist = {}; // [previousMovePieceType][previousMoveTo]

    public:

    inline i32 total(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 2> moves) 
    {
        float total = mMainHist[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight();

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                total += mContHist[pt][move.to()] * contHistoryWeight();
            }

        return total;
    }

    inline void update(i32 bonus, bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 2> moves)
    {
        mMainHist[enemyAttacksOrigin][enemyAttacksDst] 
            = std::clamp(mMainHist[enemyAttacksOrigin][enemyAttacksDst] + bonus, -historyMax(), historyMax());

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                mContHist[pt][move.to()] = std::clamp(mContHist[pt][move.to()] + bonus, -historyMax(), historyMax());
            }
    }

}; // HistoryEntry
