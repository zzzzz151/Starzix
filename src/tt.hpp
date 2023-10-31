#pragma once

// clang-format off

const int TT_SIZE_MB = 64; // default

const uint8_t INVALID_BOUND = 0, EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    uint64_t zobristHash = 0;
    int score = 0;
    Move bestMove = NULL_MOVE;
    uint8_t depth = 0; 
    uint8_t bound = INVALID_BOUND;
};

vector<TTEntry> tt(TT_SIZE_MB * 1024 * 1024 / sizeof(TTEntry)); // initializes the elements

inline void resizeTT(int sizeMB)
{
    tt.clear();
    int numEntries = sizeMB * 1024 * 1024 / sizeof(TTEntry);
    tt.resize(numEntries);
}

inline void clearTT()
{
    memset(tt.data(), 0, sizeof(TTEntry) * tt.size());
}

inline void storeInTT(TTEntry *ttEntry, uint8_t depth, Move &bestMove, int bestScore, int plyFromRoot, int originalAlpha, int beta)
{
    ttEntry->zobristHash = board.getZobristHash();
    ttEntry->depth = depth;
    ttEntry->bestMove = bestMove;

    ttEntry->score = bestScore;
    if (abs(bestScore) >= MIN_MATE_SCORE) 
        ttEntry->score += bestScore < 0 ? -plyFromRoot : plyFromRoot;

    if (bestScore <= originalAlpha) 
        ttEntry->bound = UPPER_BOUND;
    else if (bestScore >= beta) 
        ttEntry->bound = LOWER_BOUND;
    else 
        ttEntry->bound = EXACT;
}
