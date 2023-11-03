#pragma once

// clang-format off

const uint16_t TT_SIZE_MB = 64; // default

const uint8_t INVALID_BOUND = 0, EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    uint64_t zobristHash = 0;
    int16_t score = 0;
    Move bestMove = NULL_MOVE;
    uint8_t depth = 0; 
    uint8_t bound = INVALID_BOUND;
};

vector<TTEntry> tt(TT_SIZE_MB * 1024 * 1024 / sizeof(TTEntry)); // initializes the elements

inline void resizeTT(uint16_t sizeMB)
{
    tt.clear();
    uint32_t numEntries = sizeMB * 1024 * 1024 / sizeof(TTEntry);
    tt.resize(numEntries);
}

inline void clearTT()
{
    memset(tt.data(), 0, sizeof(TTEntry) * tt.size());
}

inline int16_t adjustScoreFromTT(TTEntry *ttEntry, int plyFromRoot)
{
    if (ttEntry->score >= MIN_MATE_SCORE)
        return ttEntry->score - plyFromRoot;
    else if (ttEntry->score <= -MIN_MATE_SCORE)
        return ttEntry->score + plyFromRoot;

    return ttEntry->score;
}

inline pair<TTEntry*, bool> probeTT(int depth, int16_t alpha, int16_t beta, int plyFromRoot, Move singularMove = NULL_MOVE)
{
    TTEntry *ttEntry = &(tt[board.getZobristHash() % tt.size()]);

    bool shouldCutoff = plyFromRoot > 0 
                        && ttEntry->zobristHash == board.getZobristHash() 
                        && ttEntry->depth >= depth 
                        && singularMove == NULL_MOVE
                        && (ttEntry->bound == EXACT 
                        || (ttEntry->bound == LOWER_BOUND && ttEntry->score >= beta) 
                        || (ttEntry->bound == UPPER_BOUND && ttEntry->score <= alpha));

    return { ttEntry, shouldCutoff };
}

inline void storeInTT(TTEntry *ttEntry, int depth, Move bestMove, int16_t bestScore, int plyFromRoot, int16_t originalAlpha, int16_t beta)
{
    assert(depth >= 0 && depth <= 255);

    ttEntry->zobristHash = board.getZobristHash();
    ttEntry->depth = depth;
    ttEntry->bestMove = bestMove;
    ttEntry->score = bestScore;

    // Adjust mate scores based on ply
    if (bestScore >= MIN_MATE_SCORE)
        ttEntry->score += plyFromRoot;
    else if (bestScore <= -MIN_MATE_SCORE)
        ttEntry->score -= plyFromRoot;

    if (bestScore <= originalAlpha) 
        ttEntry->bound = UPPER_BOUND;
    else if (bestScore >= beta) 
        ttEntry->bound = LOWER_BOUND;
    else 
        ttEntry->bound = EXACT;
}
