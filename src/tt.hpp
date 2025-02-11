// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include <cmath>

enum class Bound : u8 {
    None = 0, Exact = 1, Lower = 2, Upper = 3
};

struct TTEntry
{
public:

    u64 zobristHash = 0;
    u8 depth = 0;
    i16 score = 0;
    Bound bound = Bound::None;
    u16 move = MOVE_NONE.asU16();

    constexpr void update(
        const u64 newHash,
        const u8 newDepth,
        const i16 newScore,
        const i16 ply,
        const Bound newBound,
        const Move newMove)
    {
        this->zobristHash = newHash;

        this->depth = newDepth;

        this->score = newScore >= MIN_MATE_SCORE  ? newScore + ply
                    : newScore <= -MIN_MATE_SCORE ? newScore - ply
                    : newScore;

        this->bound = newBound;

        if (this->zobristHash != newHash || newMove != MOVE_NONE)
            this->move = newMove.asU16();
    }

} __attribute__((packed)); // struct TTEntry

static_assert(sizeof(TTEntry) == 8 + 2 + 2 + 1 + 1);

struct ParsedTTEntry
{
public:

    i32 depth = 0;
    i32 score = 0;
    Bound bound = Bound::None;
    Move move = MOVE_NONE;

    constexpr static std::optional<ParsedTTEntry> parse(
        const TTEntry& ttEntry, const u64 zobristHash, const i16 ply)
    {
        if (ttEntry.zobristHash != zobristHash)
            return std::nullopt;

        ParsedTTEntry parsedEntry;

        parsedEntry.depth = ttEntry.depth;

        parsedEntry.score = ttEntry.score >= MIN_MATE_SCORE  ? ttEntry.score - ply
                          : ttEntry.score <= -MIN_MATE_SCORE ? ttEntry.score + ply
                          : ttEntry.score;

        parsedEntry.bound = ttEntry.bound;

        parsedEntry.move = Move(ttEntry.move);

        return parsedEntry;
    }

}; // struct ParsedTTEntry

inline void printTTSize(const std::vector<TTEntry>& tt)
{
    const size_t bytes = tt.size() * sizeof(TTEntry);
    const double mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    const size_t mebibytesRounded = static_cast<size_t>(round(mebibytes));

    std::cout << "info string TT size " << mebibytesRounded << " MiB"
              << " (" << tt.size() << " entries)"
              << std::endl;
}

constexpr void resizeTT(std::vector<TTEntry>& tt, const size_t newMebibytes)
{
    const size_t numEntries = newMebibytes * 1024 * 1024 / sizeof(TTEntry);
    tt.clear(); // Remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

constexpr void resetTT(std::vector<TTEntry>& tt)
{
    const auto numEntries = tt.size();
    tt.clear(); // Remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

constexpr TTEntry& getEntry(std::vector<TTEntry>& tt, const u64 zobristHash)
{
    assert(tt.size() > 0);

    const size_t idx = static_cast<size_t>(
        (static_cast<u128>(zobristHash) * static_cast<u128>(tt.size())) >> 64
    );

    return tt[idx];
}
