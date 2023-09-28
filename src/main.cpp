#ifndef MAIN_CPP
#define MAIN_CPP

// clang-format off
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
    int numRows = sizeof(lmrTable) / sizeof(lmrTable[0]);
    int numCols = sizeof(lmrTable[0]) / sizeof(lmrTable[0][0]); 

    for (int depth = 0; depth < numRows; depth++)
        for (int move = 0; move < numCols; move++)
            // log(x) is ln(x)
            lmrTable[depth][move] = depth == 0 || move == 0 ? 0 : round(LMR_BASE + log(depth) * log(move) / LMR_DIVISOR);
}

int main()
{
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();
    initTT();
    initLmrTable();
    uciLoop();
    return 0;
}

#endif
