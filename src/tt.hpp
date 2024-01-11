#pragma once

// clang-format off

#include <cstring> // for memset()

const u16 TT_DEFAULT_SIZE_MB = 32;
u16 ttSizeMB = TT_DEFAULT_SIZE_MB;

enum class Bound {
    INVALID = 0,
    EXACT = 1,
    LOWER = 2,
    UPPER = 3
};

struct TTEntry
{
    u64 zobristHash = 0;
    i16 score = 0;
    Move bestMove = MOVE_NONE;
    u8 depth = 0; 
    u8 boundAndAge = 0; // lowest 2 bits for bound, highest 6 bits for age

    inline i16 adjustedScore(u16 ply)
    {
        if (score >= MIN_MATE_SCORE)
            return score - ply;
        if (score <= -MIN_MATE_SCORE)
            return score + ply;
        return score;
    }

    inline Bound getBound() {
         return (Bound)(boundAndAge & 0b0000'0011); 
    }

    inline void setBound(Bound newBound) { 
        boundAndAge = (boundAndAge & 0b1111'1100) | (u8)newBound; 
    }

    inline u8 getAge() { 
        return boundAndAge >> 2;
    }

    inline void setAge(u8 newAge) { 
        assert(newAge < 63);
        boundAndAge &= 0b0000'0011;
        boundAndAge |= (newAge << 2);
    }

    inline void setBoundAndAge(Bound bound, u8 age) {
        assert(age < 63);
        boundAndAge = (age << 2) | (u8)bound;
    }

} __attribute__((packed)); 

struct TT
{
    std::vector<TTEntry> tt = {};
    u8 age = 0;

    inline TT() = default;

    inline TT(bool init) {
        if (init) resize();
    }

    inline void resize()
    {
        tt.clear();
        u32 numEntries = ttSizeMB * 1024 * 1024 / sizeof(TTEntry);
        tt.resize(numEntries);
        std::cout << "TT size: " << ttSizeMB << " MB (" << numEntries << " entries)" << std::endl;
    }

    inline void reset()
    {
        memset(tt.data(), 0, sizeof(TTEntry) * tt.size());
        age = 0;
    }

    inline TTEntry* probe(u64 zobristHash) {
        return &tt[zobristHash % tt.size()];
    }

    inline bool cutoff(TTEntry *ttEntry, u64 zobristHash, u8 depth, u8 ply, i16 alpha, i16 beta) {
        return ply > 0 
               && ttEntry->zobristHash == zobristHash
               && ttEntry->depth >= depth 
               && (ttEntry->getBound() == Bound::EXACT
               || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
               || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha));
    }

    inline void store(TTEntry *ttEntry, u64 zobristHash, u8 depth, u8 ply, i16 score, Move bestMove, Bound bound)
    {
        ttEntry->zobristHash = zobristHash;
        ttEntry->depth = depth;
        ttEntry->score = score;
        if (bestMove != MOVE_NONE) 
            ttEntry->bestMove = bestMove;
        ttEntry->setBound(bound);

        // Adjust mate scores based on ply
        if (ttEntry->score >= MIN_MATE_SCORE)
            ttEntry->score += ply;
        else if (ttEntry->score <= -MIN_MATE_SCORE)
            ttEntry->score -= ply;

    }

    inline void store(TTEntry *ttEntry, u64 zobristHash, u8 depth, u8 ply, i16 score, Move bestMove, i16 originalAlpha, i16 beta)
    {
        Bound bound = score <= originalAlpha ? Bound::UPPER : score >= beta ? Bound::LOWER : Bound::EXACT;
        store(ttEntry, zobristHash, depth, ply, score, bestMove, bound);
    }
};

