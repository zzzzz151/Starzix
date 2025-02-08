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
        const Bound newBound,
        const Move newMove)
    {
        this->zobristHash = newHash;
        this->depth = newDepth;
        this->score = newScore;
        this->bound = newBound;

        if (this->zobristHash != newHash || newMove != MOVE_NONE)
            this->move = newMove.asU16();
    }

} __attribute__((packed)); // struct TTEntry

static_assert(sizeof(TTEntry) == 8 + 2 + 2 + 1 + 1);

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
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

constexpr void resetTT(std::vector<TTEntry>& tt)
{
    const auto numEntries = tt.size();
    tt.clear(); // remove all elements
    tt.resize(numEntries);
    tt.shrink_to_fit();
}

constexpr size_t ttEntryIndex(const u64 zobristHash, const size_t numEntries)
{
    return (static_cast<u128>(zobristHash) * static_cast<u128>(numEntries)) >> 64;
}
