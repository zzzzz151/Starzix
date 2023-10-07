#pragma once

// clang-format off

int TT_SIZE_MB = 64; // default and current

const uint8_t INVALID = 0, EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    uint64_t zobristHash = 0;
    int score = 0;
    Move bestMove = NULL_MOVE;
    uint8_t depth = 0;
    uint8_t type = INVALID;
};
vector<TTEntry> tt;

inline void initTT()
{
    int numEntries = (TT_SIZE_MB * 1024 * 1024) / sizeof(TTEntry);
    tt.clear();
    tt.resize(numEntries);
    //cout << "TT size = " << TT_SIZE_MB << "MB => " << numEntries << " entries" << endl;
}

inline void clearTT()
{
    memset(tt.data(), 0, sizeof(TTEntry) * tt.size());
}

inline void storeInTT(TTEntry *ttEntry, uint8_t depth, Move &bestMove, int bestScore, int plyFromRoot, int originalAlpha, int beta)
{
    ttEntry->zobristHash = board.zobristHash();
    ttEntry->depth = depth;
    ttEntry->bestMove = bestMove;

    ttEntry->score = bestScore;
    if (abs(bestScore) >= MIN_MATE_SCORE) 
        ttEntry->score += bestScore < 0 ? -plyFromRoot : plyFromRoot;

    if (bestScore <= originalAlpha) 
        ttEntry->type = UPPER_BOUND;
    else if (bestScore >= beta) 
        ttEntry->type = LOWER_BOUND;
    else 
        ttEntry->type = EXACT;
}
