// clang-format off

#pragma once

inline void updateHistory(i16 &history, i32 bonus) {
    history = std::clamp((i32)history + bonus, -historyMax(), historyMax());
}

inline void updateHistory(i16* history, i32 bonus) {
    updateHistory(*history, bonus);
}

struct HistoryEntry {
    public:

    MultiArray<i16, 2, 2>  mainHist  = {}; // [enemyAttacksOrigin][enemyAttacksDestination]
    MultiArray<i16, 6, 64> contHist  = {}; // [previousMovePieceType][previousMoveTo]
    std::array<i16, 7>     noisyHist = {}; // [pieceTypeCaptured]

    inline i32 quietHistory(bool enemyAttacksOrigin, bool enemyAttacksDst, std::array<Move, 2> moves) 
    {
        float total = nainHist[enemyAttacksOrigin][enemyAttacksDst] * mainHistoryWeight();

        for (Move move : moves) 
            if (move != MOVE_NONE) {
                int pt = (int)move.pieceType();
                total += contHist[pt][move.to()] * contHistoryWeight();
            }

        return total;
    }

}; // HistoryEntry
