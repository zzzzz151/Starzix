#pragma once

// clang-format off

const uint16_t TT_DEFAULT_SIZE_MB = 64;
const uint8_t INVALID_BOUND = 0, EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
uint8_t ttAge = 0;

struct TTEntry
{
    uint64_t zobristHash = 0;
    int16_t score = 0;
    Move bestMove = NULL_MOVE;
    uint8_t depth = 0; 
    uint8_t boundAndAge = 0; // lowest 2 bits for bound, highest 6 bits for age

    inline int16_t adjustedScore(int plyFromRoot)
    {
        if (score >= MIN_MATE_SCORE)
            return score - plyFromRoot;
        else if (score <= -MIN_MATE_SCORE)
            return score + plyFromRoot;
        return score;
    }

    inline uint8_t getBound() {
         return boundAndAge & 0b0000'0011; 
    }

    inline void setBound(uint8_t newBound) { 
        boundAndAge = (boundAndAge & 0b1111'1100) | newBound; 
    }

    inline uint8_t getAge() { 
        return boundAndAge & 0b1111'1100;
    }

    inline void setAge(uint8_t newAge) { 
        boundAndAge &= 0b0000'0011;
        boundAndAge |= (newAge << 2);
    }

    inline void setBoundAndAge(uint8_t bound, uint8_t age) {
        boundAndAge = (age << 2) | bound;
    }

} __attribute__((packed)); 

vector<TTEntry> tt(TT_DEFAULT_SIZE_MB * 1024 * 1024 / sizeof(TTEntry)); // initializes the elements

inline void resizeTT(uint16_t sizeMB)
{
    tt.clear();
    uint32_t numEntries = sizeMB * 1024 * 1024 / sizeof(TTEntry);
    tt.resize(numEntries);
}

inline void clearTT()
{
    memset(tt.data(), 0, sizeof(TTEntry) * tt.size());
    ttAge = 0;
}

inline pair<TTEntry*, bool> probeTT(uint64_t zobristHash, int depth, int16_t alpha, int16_t beta, int plyFromRoot, Move singularMove = NULL_MOVE)
{
    TTEntry *ttEntry = &(tt[zobristHash % tt.size()]);
    uint8_t ttEntryBound = ttEntry->getBound();

    bool shouldCutoff = plyFromRoot > 0 
                        && ttEntry->zobristHash == zobristHash
                        && ttEntry->depth >= depth 
                        && singularMove == NULL_MOVE
                        && (ttEntryBound == EXACT 
                        || (ttEntryBound == LOWER_BOUND && ttEntry->score >= beta) 
                        || (ttEntryBound == UPPER_BOUND && ttEntry->score <= alpha));

    return { ttEntry, shouldCutoff };
}

inline void storeInTT(TTEntry *ttEntry, uint64_t zobristHash, int depth, Move bestMove, int16_t bestScore, int plyFromRoot, int16_t originalAlpha, int16_t beta)
{
    if (bestMove != NULL_MOVE)
        ttEntry->bestMove = bestMove;

    uint8_t bound = EXACT;
    if (bestScore <= originalAlpha) 
        bound = UPPER_BOUND;
    else if (bestScore >= beta) 
        bound = LOWER_BOUND;

    // replacement scheme
    if (ttEntry->zobristHash != 0
    && bound != EXACT 
    && ttEntry->depth >= depth + (zobristHash == ttEntry->zobristHash ? 3 : 0)
    && ttEntry->getAge() == ttAge)
        return;

    ttEntry->zobristHash = zobristHash;
    ttEntry->depth = depth;
    ttEntry->score = bestScore;
    ttEntry->setBoundAndAge(bound, ttAge);

    // Adjust mate scores based on ply
    if (bestScore >= MIN_MATE_SCORE)
        ttEntry->score += plyFromRoot;
    else if (bestScore <= -MIN_MATE_SCORE)
        ttEntry->score -= plyFromRoot;

}
