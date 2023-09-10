#ifndef MAIN_CPP
#define MAIN_CPP

#include "search.hpp"
#include "uci.hpp"

inline void initTT()
{
    NUM_TT_ENTRIES = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
    TT.clear();
    TT.resize(NUM_TT_ENTRIES);
    // cout << "TT_SIZE_MB = " << TT_SIZE_MB << " => " << NUM_TT_ENTRIES << " entries" << endl;
}

inline void initLmrTable()
{
    for (int depth = 0; depth < lmrTableNumRows; depth++)
        for (int move = 0; move < lmrTableNumCols; move++)
            lmrTable[depth][move] = depth == 0 || move == 0 ? 0 : LMR_BASE + log(depth) * log(move) / LMR_DIVISOR; // ln(x)
}

int main()
{
    initTT();
    initLmrTable();
    uciLoop();
    return 0;
}

#endif
