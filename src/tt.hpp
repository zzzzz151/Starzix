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
        return boundAndAge & 0b1111'1100;
    }

    inline void setAge(u8 newAge) { 
        boundAndAge &= 0b0000'0011;
        boundAndAge |= (newAge << 2);
    }

    inline void setBoundAndAge(Bound bound, u8 age) {
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

    inline std::pair<TTEntry*, bool> probe(u64 zobristHash, int depth, u16 ply, i16 alpha, i16 beta)
    {
        TTEntry *ttEntry = &(tt[zobristHash % tt.size()]);
        Bound ttEntryBound = ttEntry->getBound();

        bool cutoff = ply > 0 
                      && ttEntry->zobristHash == zobristHash
                      && ttEntry->depth >= depth 
                      && (ttEntryBound == Bound::EXACT
                      || (ttEntryBound == Bound::LOWER && ttEntry->score >= beta) 
                      || (ttEntryBound == Bound::UPPER && ttEntry->score <= alpha));

        return { ttEntry, cutoff };
    }

    inline void store(TTEntry *ttEntry, u64 zobristHash, i32 depth, u16 ply, i16 score, Move bestMove, i16 originalAlpha, i16 beta)
    {
        assert(depth >= 0);

        Bound bound = Bound::EXACT;
        if (score <= originalAlpha) 
            bound = Bound::UPPER;
        else if (score >= beta) 
            bound = Bound::LOWER;

        ttEntry->zobristHash = zobristHash;
        ttEntry->depth = depth;
        ttEntry->score = score;
        ttEntry->setBoundAndAge(bound, age);
        if (bestMove != MOVE_NONE) ttEntry->bestMove = bestMove;

        // Adjust mate scores based on ply
        if (ttEntry->score >= MIN_MATE_SCORE)
            ttEntry->score += ply;
        else if (ttEntry->score <= -MIN_MATE_SCORE)
            ttEntry->score -= ply;

    }
};

