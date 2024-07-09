// clang-format off

#pragma once

struct HistoryEntry {
    private:
    i16 mainHistory = 0;
    MultiArray<i16, 6, 64> contHist = {}; // [previousMovePieceType][previousMoveTo]

    public:

    inline i32 total(Move prevMove) {
        if (prevMove == MOVE_NONE) return (float)mainHistory * mainHistoryWeight();

        int pt = (int)prevMove.pieceType();

        return (float)mainHistory                 * mainHistoryWeight() 
             + (float)contHist[pt][prevMove.to()] * contHistoryWeight();
    }

    inline void update(i32 bonus, Move prevMove) {
        mainHistory = std::clamp((i32)mainHistory + bonus, -historyMax(), historyMax());

        if (prevMove != MOVE_NONE) {
            int pt = (int)prevMove.pieceType();
            contHist[pt][prevMove.to()] = std::clamp((i32)contHist[pt][prevMove.to()] + bonus, -historyMax(), historyMax());
        }
    }

}; // HistoryEntry