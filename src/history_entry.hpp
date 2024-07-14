// clang-format off

#pragma once

std::array<float, 3> CONT_HISTORY_WEIGHTS = { 
    contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight() 
};

struct HistoryEntry {
    private:
    MultiArray<i16, 2, 2>  mMainHist = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> mContHist = {}; // [previousMovePieceType][previousMoveTo]

    public:

    inline i32 total(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves) 
    {
        float total = mMainHist[enemyAttacksOrigin][enemyAttacksDst];

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                total += mContHist[pt][move.to()];
            }

        return total;
    }

    inline void update(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 3> moves, i32 bonus)
    {
        auto updateHistory = [](i16 &history, i32 bonus) -> void {
            history += bonus - abs(bonus) * (i32)history / historyMax();
            assert(history >= -historyMax() && history <= historyMax());
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

}; // HistoryEntry
