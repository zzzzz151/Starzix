// clang-format off

#pragma once

#include "utils.hpp"
#include "move.hpp"
#include <cmath>
#include <tuple>

enum class Bound : u8 {
    None = 0, Exact = 1, Lower = 2, Upper = 3
};

// ttDepth, ttScore, ttBound, ttMove
constexpr std::tuple<std::optional<i32>, std::optional<i32>, Bound, Move> NO_TT_ENTRY_DATA = {
    std::nullopt, std::nullopt, Bound::None, MOVE_NONE
};

struct TTEntry
{
private:

    u64 mZobristHash = 0;
    u8 mDepth = 0;
    i16 mScore = 0;
    Bound mBound = Bound::None;
    u16 mMove = 0;

public:

    // ttDepth, ttScore, ttBound, ttMove
    constexpr std::tuple<std::optional<i32>, std::optional<i32>, Bound, Move> get(
        const u64 zobristHash, const i16 ply) const
    {
        if (mZobristHash != zobristHash || mBound == Bound::None)
            return NO_TT_ENTRY_DATA;

        const i16 score = mScore >= MIN_MATE_SCORE  ? mScore - ply
                        : mScore <= -MIN_MATE_SCORE ? mScore + ply
                        : mScore;

        return {
            static_cast<i32>(mDepth),
            static_cast<i32>(score),
            mBound,
            Move(mMove)
        };
    }

    constexpr void update(
        const u64 newHash,
        const u8 newDepth,
        const i16 newScore,
        const i16 ply,
        const Bound newBound,
        const Move newMove)
    {
        mZobristHash = newHash;

        mDepth = newDepth;

        mScore = newScore >= MIN_MATE_SCORE  ? newScore + ply
               : newScore <= -MIN_MATE_SCORE ? newScore - ply
               : newScore;

        mBound = newBound;

        if (mZobristHash != newHash || newMove)
            mMove = newMove.asU16();
    }

} __attribute__((packed)); // struct TTEntry

static_assert(sizeof(TTEntry) == 8 + 2 + 2 + 1 + 1);

inline void printTTSize(const std::vector<TTEntry>& tt)
{
    const size_t bytes = tt.size() * sizeof(TTEntry);
    const double mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    const size_t mebibytesRounded = static_cast<size_t>(llround(mebibytes));

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
