// clang-format off

#pragma once

enum class Bound {
    NONE = 0, EXACT = 1, LOWER = 2, UPPER = 3
};

struct TTEntry {
    public:
    
    u64 zobristHash = 0;
    u8 depth = 0; 
    i16 score = 0;
    u16 move = MOVE_NONE.encoded();
    u8 boundAndAge = 0; // lowest 2 bits for bound, highest 6 bits for age

    inline i16 adjustedScore(i16 ply) {
        return score >= MIN_MATE_SCORE  ? score - ply
             : score <= -MIN_MATE_SCORE ? score + ply
             : score;
    }

    inline Bound bound() {
         return (Bound)(boundAndAge & 0b0000'0011); 
    }

    inline void update(u64 zobristHash, u8 depth, i16 ply, i16 score, Move bestMove, Bound bound)
    {
        this->zobristHash = zobristHash;
        this->depth = depth;
        this->boundAndAge = (this->boundAndAge & 0b1111'1100) | (u8)bound; 

        this->score = score >= MIN_MATE_SCORE  ? score + ply 
                    : score <= -MIN_MATE_SCORE ? score - ply 
                    : score;

        if (bestMove != MOVE_NONE) this->move = bestMove.encoded();
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
    newSizeMB = std::clamp(newSizeMB, (i64)1, (i64)65536);
    u64 numEntries = (u64)newSizeMB * 1024 * 1024 / (u64)sizeof(TTEntry); 

    tt.clear(); // remove all elements
    tt.resize(round(numEntries));
    tt.shrink_to_fit();
}

inline void resetTT(std::vector<TTEntry> &tt) {
    auto numEntries = tt.size();
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}