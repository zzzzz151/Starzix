// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include "search_params.hpp"

enum class Bound {
    NONE = 0, EXACT = 1, LOWER = 2, UPPER = 3
};

struct TTEntry {
    public:
    
    u64 zobristHash = 0;
    i16 score = 0;
    u16 move = MOVE_NONE.encoded();
    u16 depthBoundAge; // from lowest to highest bits: 7 for depth, 2 for bound, 7 for age

    constexpr i32 depth() const {
        return depthBoundAge & 0b1111111;
    }

    constexpr Bound bound() const {
         return Bound((depthBoundAge >> 7) & 0b11);
    }

    constexpr void adjustScore(const i16 ply) 
    {
        if (score >= MIN_MATE_SCORE)
            score -= ply;
        else if (score <= -MIN_MATE_SCORE)
            score += ply;
    }

    constexpr void update(const u64 newZobristHash, const u8 newDepth, const i16 ply, 
        const i16 newScore, const Move newBestMove, const Bound newBound)
    {
        assert((newDepth & 0b1000'0000) == 0);

        // Update TT entry if
        if (this->zobristHash != newZobristHash // this entry is empty or a TT collision
        || newBound == Bound::EXACT             // or new bound is exact
        || this->depth() < (i32)newDepth + 4)   // or new depth isn't much lower
        {
            this->zobristHash = newZobristHash;

            this->depthBoundAge = newDepth;
            this->depthBoundAge |= (u16)newBound << 7;

            this->score = newScore >= MIN_MATE_SCORE  ? newScore + ply 
                        : newScore <= -MIN_MATE_SCORE ? newScore - ply 
                        : newScore;
        }

        // Update entry's best move if
        if (this->zobristHash != newZobristHash // this entry is empty or a TT collision
        || Move(this->move) == MOVE_NONE        // or this TT entry doesn't have a move
        || newBound != Bound::UPPER)            // or if it has a move, if the new move is not a fail low
            this->move = newBestMove.encoded();
    }

} __attribute__((packed)); // struct TTEntry

static_assert(sizeof(TTEntry) == 8 + 2 + 2 + 2);

constexpr u64 TTEntryIndex(const u64 zobristHash, const auto numEntries) 
{
    return ((u128)zobristHash * (u128)numEntries) >> 64;
}

inline void printTTSize(const std::vector<TTEntry> &tt) 
{
    const double bytes = u64(tt.size()) * (u64)sizeof(TTEntry);
    const double megabytes = bytes / (1024.0 * 1024.0);

    std::cout << "TT size: " << round(megabytes) << " MB"
              << " (" << tt.size() << " entries)" 
              << std::endl;
}

constexpr void resizeTT(std::vector<TTEntry> &tt, i64 newSizeMB) 
{
    newSizeMB = std::clamp(newSizeMB, (i64)1, (i64)65536);
    const u64 numEntries = (u64)newSizeMB * 1024 * 1024 / (u64)sizeof(TTEntry); 

    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

constexpr void resetTT(std::vector<TTEntry> &tt) 
{
    const auto numEntries = tt.size();
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}
