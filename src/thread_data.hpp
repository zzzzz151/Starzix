// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "nnue.hpp"
#include "search_params.hpp"
#include <mutex>
#include <condition_variable>
#include <algorithm>

struct PlyData {
public:

    ArrayVec<Move, MAX_DEPTH + 1> pvLine;
    std::optional<i32> eval = std::nullopt;
};

enum class ThreadState : i32 {
    Sleeping, Searching, ExitAsap, Exited
};

struct ThreadData {
public:

    Position pos;

    i32 score = 0;

    u64 nodes = 0;
    size_t maxPlyReached = 0;

    std::array<PlyData, MAX_DEPTH + 1> pliesData; // [ply]

    std::array<u64, 1ULL << 17> nodesByMove; // [Move.asU16()]

    std::array<nnue::BothAccumulators, MAX_DEPTH + 1> bothAccsStack;
    size_t bothAccsIdx = 0;

    nnue::FinnyTable finnyTable; // [color][mirrorVAxis][inputBucket]

    // Threading stuff
    ThreadState threadState = ThreadState::Sleeping;
    std::mutex mutex;
    std::condition_variable cv;

}; // struct ThreadData

inline void wakeThread(ThreadData* td, const ThreadState newState)
{
    std::unique_lock<std::mutex> lock(td->mutex);
    td->threadState = newState;
    lock.unlock();
    td->cv.notify_one();
}

constexpr void makeMove(ThreadData& td, const Move move, const size_t newPly)
{
    td.pos.makeMove(move);
    td.nodes++;

    // update seldepth
    td.maxPlyReached = std::max(td.maxPlyReached, newPly);

    td.pliesData[newPly].pvLine.clear();
    td.pliesData[newPly].eval = std::nullopt;

    if (move != MOVE_NONE)
    {
        td.bothAccsIdx++;
        td.bothAccsStack[td.bothAccsIdx].mUpdated = false;
    }
}

constexpr std::optional<i32> updateBothAccsAndEval(ThreadData& td, const size_t ply)
{
    nnue::BothAccumulators& bothAccs = td.bothAccsStack[td.bothAccsIdx];
    std::optional<i32>& eval = td.pliesData[ply].eval;

    if (!bothAccs.mUpdated)
    {
        assert(td.bothAccsIdx > 0);
        bothAccs.update(td.bothAccsStack[td.bothAccsIdx - 1], td.pos, td.finnyTable);
    }

    if (td.pos.inCheck())
        eval = std::nullopt;
    else if (!eval)
    {
        assert(bothAccs == nnue::BothAccumulators(td.pos));

        eval = nnue::evaluate(bothAccs, td.pos.sideToMove());

        // Clamp eval to avoid invalid values and checkmate values
        eval = std::clamp<i32>(*eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
    }

    return eval;
}
