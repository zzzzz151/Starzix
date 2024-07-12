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
        auto updateHistory = [](i16 &history, i32 bonus) -> void {
            history += bonus - abs(bonus) * (i32)history / historyMax();
            assert(history >= -historyMax() && history <= historyMax());
        };

        updateHistory(mMainHist[enemyAttacksOrigin][enemyAttacksDst], bonus);

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                updateHistory(mContHist[pt][move.to()], bonus);
            }
    }

}; // HistoryEntry
