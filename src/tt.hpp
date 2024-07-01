// clang-format off

#pragma once

enum class Bound {
    NONE = 0,
    EXACT = 1,
    LOWER = 2,
    UPPER = 3
};

struct TTEntry {
    public:
    
    u64 zobristHash = 0;
    u8 depth = 0; 
    i16 score = 0;
    u16 move = MOVE_NONE.encoded();
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

    inline void update(u64 zobristHash, u8 depth, u8 ply, i16 score, Move bestMove, Bound bound)
    {
        this->zobristHash = zobristHash;
        this->depth = depth;
        this->setBound(bound);
        this->score = score >= MIN_MATE_SCORE 
                      ? score + (i16)ply 
                      : score <= -MIN_MATE_SCORE 
                      ? score - (i16)ply 
                      : score;

        // If bound != Bound::UPPER, then bestMove != MOVE_NONE
        if (Move(this->move) == MOVE_NONE || bound != Bound::UPPER)
            this->move = bestMove.encoded();
    }

    inline void update(u64 zobristHash, u8 depth, u8 ply,  i16 score, 
                       Move bestMove, i16 originalAlpha, i16 beta)
    {
        Bound bound = score <= originalAlpha ? Bound::UPPER 
                      : score >= beta ? Bound::LOWER 
                      : Bound::EXACT;

        update(zobristHash, depth, ply, score, bestMove, bound);
    }

} __attribute__((packed)); // struct TTEntry

static_assert(sizeof(TTEntry) == 8 + 1 + 2 + 2 + 1);

inline u64 TTEntryIndex(u64 zobristHash, auto numEntries) 
{
    return ((u128)zobristHash * (u128)numEntries) >> 64;
}

inline void printTTSize(std::vector<TTEntry> &tt) 
{
    double bytes = (u64)tt.size() * (u64)sizeof(TTEntry);
    double megabytes = bytes / (1024.0 * 1024.0);

    std::cout << "TT size: " << round(megabytes) << " MB"
            << " (" << tt.size() << " entries)" 
            << std::endl;
}

inline void resizeTT(std::vector<TTEntry> &tt, i64 newSizeMB) 
{
    u64 numEntries = std::clamp(newSizeMB, (i64)1, (i64)65536) * (u64)1024 * (u64)1024 / (u64)sizeof(TTEntry); 
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

inline void resetTT(std::vector<TTEntry> &tt) {
    auto numEntries = tt.size();
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}