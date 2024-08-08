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

    inline void adjustScore(i16 ply) 
    {
        if (score >= MIN_MATE_SCORE)
            score -= ply;
        else if (score <= -MIN_MATE_SCORE)
            score += ply;
    }

    inline Bound bound() {
         return (Bound)(boundAndAge & 0b0000'0011); 
    }

    inline void update(u64 newZobristHash, u8 newDepth, i16 ply, i16 newScore, Move newBestMove, Bound newBound)
    {
        /// Update TT entry if
        if (this->zobristHash != newZobristHash  // this is a TT collision
        || newBound == Bound::EXACT              // or new bound is exact
        || (i32)newDepth + 4 > (i32)this->depth) // or new depth is much higher
        {
            this->zobristHash = newZobristHash;
            this->depth = newDepth;
            this->boundAndAge = (this->boundAndAge & 0b1111'1100) | (u8)newBound; 

            this->score = newScore >= MIN_MATE_SCORE  ? newScore + ply 
                        : newScore <= -MIN_MATE_SCORE ? newScore - ply 
                        : newScore;
        }

        // Save new best move in TT if
        if (this->zobristHash != newZobristHash // this is a TT collision
        || Move(this->move) == MOVE_NONE        // or this TT entry doesn't have a move
        || newBound != Bound::UPPER)            // or if it has a move, if the new move is not a fail low
            this->move = newBestMove.encoded();
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
