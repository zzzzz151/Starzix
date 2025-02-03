// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "nnue.hpp"
#include "search_params.hpp"
#include "thread_data.hpp"

struct SearchConfig {
private:

    i32 maxDepth = MAX_DEPTH;

public:

    u64 maxNodes = std::numeric_limits<u64>::max();
    std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
    u64 hardMs = std::numeric_limits<u64>::max();
    u64 softMs = std::numeric_limits<u64>::max();
    bool printInfo = true;

    constexpr void setMaxDepth(const i32 newMaxDepth)
    {
        this->maxDepth = std::clamp(newMaxDepth, 1, MAX_DEPTH);
    }

}; // struct SearchConfig

class Searcher {
private:

    std::vector<ThreadData*> mThreadsData   = { };
    std::vector<std::thread> mNativeThreads = { };

    SearchConfig mSearchConfig;

    std::atomic<bool> mStopSearch = false;

    constexpr const ThreadData* mainThreadData() const
    {
        assert(!mThreadsData.empty());
        return mThreadsData[0];
    }

    constexpr ThreadData* mainThreadData()
    {
        assert(!mThreadsData.empty());
        return mThreadsData[0];
    }

public:

    inline Searcher() {
        setThreads(1);
    }

    inline ~Searcher() {
        setThreads(0);
    }

    constexpr void ucinewgame()
    {
        mainThreadData()->pos = START_POS;
        mainThreadData()->pliesData[0].pvLine.clear(); // reset best move at root

        for (ThreadData* td : mThreadsData)
            td->nodes = 0;
    }

    constexpr Move bestMoveAtRoot() const
    {
        return mainThreadData()->pliesData[0].pvLine.size() > 0
             ? mainThreadData()->pliesData[0].pvLine[0]
             : MOVE_NONE;
    }

    constexpr u64 totalNodes() const
    {
        u64 nodes = 0;

        for (const ThreadData* td : mThreadsData)
            nodes += td->nodes;

        return nodes;
    }

    inline std::pair<Move, i32> search(const Position& pos, const SearchConfig& searchConfig)
    {
        mStopSearch = false;
        blockUntilSleep();

        mSearchConfig = searchConfig;

        /*
        // Init main thread's root accumulator
        nnue::BothAccumulators& bothAccs = mainThreadData()->bothAccsStack[0];
        bothAccs = nnue::BothAccumulators(pos);

        const auto initFinnyEntry = [&] (
            const Color color, const bool mirrorVAxis, const size_t inputBucket) constexpr -> void
        {
            nnue::FinnyTableEntry& finnyEntry
                = mainThreadData()->finnyTable[color][mirrorVAxis][inputBucket];

            if (mirrorVAxis == bothAccs.mMirrorVAxis[color]
            &&  inputBucket == bothAccs.mInputBucket[color])
            {
                finnyEntry.accumulator = bothAccs.mAccumulators[color];
                finnyEntry.colorBbs = pos.colorBbs();
                finnyEntry.piecesBbs = pos.piecesBbs();
            }
            else {
                finnyEntry.accumulator = nnue::NET->hiddenBiases[color];
                finnyEntry.colorBbs  = { };
                finnyEntry.piecesBbs = { };
            }
        }

        // Init main thread's finny table
        for (const Color color : EnumIter<Color>())
            for (const bool mirrorVAxis : { false, true })
                for (size_t inputBucket = 0; inputBucket < nnue::NUM_INPUT_BUCKETS; inputBucket++)
                    initFinnyEntry(color, mirrorVAxis, inputBucket);
        */

        for (ThreadData* td : mThreadsData)
            td->nodes = 0;

        for (ThreadData* td : mThreadsData)
        {
            td->pos = pos;
            td->pliesData[0] = PlyData();
            td->nodesByMove = { };
            td->bothAccsIdx = 0;

            /*
            if (td != mainThreadData())
            {
                td->accumulators[0] = bothAccs;
                td->finnyTable = mainThreadData()->finnyTable;
            }
            */

            wakeThread(td, ThreadState::Searching);
        }

        blockUntilSleep();

        return { bestMoveAtRoot(), mainThreadData()->score };
    }

private:

    inline void loopThread(ThreadData* td)
    {
        while (true) {
            std::unique_lock<std::mutex> lock(td->mutex);

            td->cv.wait(lock, [&] {
                return td->threadState != ThreadState::Sleeping;
            });

            if (td->threadState == ThreadState::Searching)
                iterativeDeepening(td);
            else if (td->threadState == ThreadState::ExitAsap)
                break;

            td->threadState = ThreadState::Sleeping;
            td->cv.notify_one();
        }

        std::unique_lock<std::mutex> lock(td->mutex);
        td->threadState = ThreadState::Exited;
        lock.unlock();
        td->cv.notify_all();
    }

    constexpr void blockUntilSleep()
    {
        for (ThreadData* td : mThreadsData)
        {
            std::unique_lock<std::mutex> lock(td->mutex);

            td->cv.wait(lock, [&] {
                return td->threadState == ThreadState::Sleeping;
            });
        }
    }

    inline void setThreads(const size_t numThreads)
    {
        blockUntilSleep();

        const Position pos = mThreadsData.empty() ? START_POS : mainThreadData()->pos;

        // Remove threads
        while (!mThreadsData.empty())
        {
            ThreadData* td = mThreadsData.back();

            wakeThread(td, ThreadState::ExitAsap);

            {
                std::unique_lock<std::mutex> lock(td->mutex);

                td->cv.wait(lock, [td] {
                    return td->threadState == ThreadState::Exited;
                });
            }

            if (mNativeThreads.back().joinable())
                mNativeThreads.back().join();

            mNativeThreads.pop_back();
            delete td, mThreadsData.pop_back();
        }

        mThreadsData.reserve(numThreads);
        mNativeThreads.reserve(numThreads);

        // Add threads
        while (mThreadsData.size() < numThreads)
        {
            ThreadData* td = new ThreadData();
            std::thread nativeThread([=, this]() mutable { loopThread(td); });

            mThreadsData.push_back(td);
            mNativeThreads.push_back(std::move(nativeThread));
        }

        mThreadsData.shrink_to_fit();
        mNativeThreads.shrink_to_fit();

        if (!mThreadsData.empty())
            mainThreadData()->pos = pos;
    }

    constexpr void iterativeDeepening([[maybe_unused]] ThreadData* td)
    {
        const auto pseudolegals = pseudolegalMoves(td->pos, MoveGenType::AllMoves, false);
        ArrayVec<Move, 256> legals = { };

        for (const Move pseudolegal : pseudolegals)
            if (isPseudolegalLegal(td->pos, pseudolegal))
                legals.push_back(pseudolegal);

        const Move bestMove = legals.size() == 0
                            ? MOVE_NONE
                            : legals[td->pos.zobristHash() % legals.size()];

        assert(td->pliesData[0].pvLine.size() == 0);
        td->pliesData[0].pvLine.push_back(bestMove);

        // If main thread, signal other threads to stop searching
        if (td == mainThreadData())
            mStopSearch = true;
    }

}; // class Searcher
