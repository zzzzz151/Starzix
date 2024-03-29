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

struct TTEntry {
    public:
    u64 zobristHash = 0;
    i16 score = 0;
    Move bestMove = MOVE_NONE;
    u8 depth = 0; 
    u8 boundAndAge = 0; // lowest 2 bits for bound, highest 6 bits for age

    inline i16 adjustedScore(i16 ply)
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
        assert(newAge <= 63);
        boundAndAge &= 0b0000'0011;
        boundAndAge |= (newAge << 2);
    }

    inline void setBoundAndAge(Bound bound, u8 age) {
        assert(age <= 63);
        boundAndAge = (age << 2) | (u8)bound;
    }

} __attribute__((packed)); // struct TTEntry

struct TT {
    public:
    std::vector<TTEntry> entries = {};
    u8 age = 0;

    inline void resize() {
        entries.clear();
        u32 numEntries = ttSizeMB * 1024 * 1024 / sizeof(TTEntry);
        entries.resize(numEntries);
        //std::cout << "TT size: " << ttSizeMB << " MB (" << numEntries << " entries)" << std::endl;
    }

    inline void reset() {
        memset(entries.data(), 0, sizeof(TTEntry) * entries.size());
        age = 0;
    }

    inline TTEntry* probe(u64 zobristHash) {
        return &entries[zobristHash % entries.size()];
    }

    inline void store(TTEntry *ttEntry, u64 zobristHash, u8 depth, u8 ply, i16 score, Move bestMove, Bound bound)
    {
        ttEntry->zobristHash = zobristHash;
        ttEntry->depth = depth;
        ttEntry->setBound(bound);
        ttEntry->score = score >= MIN_MATE_SCORE 
                         ? score + ply 
                         : score <= -MIN_MATE_SCORE 
                         ? score - ply 
                         : score;

        if (bestMove == MOVE_NONE || (ttEntry->bestMove != MOVE_NONE && bound == Bound::UPPER))
            return;

        ttEntry->bestMove = bestMove;
    }

    inline void store(TTEntry *ttEntry, u64 zobristHash, u8 depth, u8 ply, 
                      i16 score, Move bestMove, i16 originalAlpha, i16 beta)
    {
        Bound bound = score <= originalAlpha ? Bound::UPPER : score >= beta ? Bound::LOWER : Bound::EXACT;
        store(ttEntry, zobristHash, depth, ply, score, bestMove, bound);
    }
}; // struct TT

