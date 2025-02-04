// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "nnue.hpp"
#include "search_params.hpp"
#include "thread_data.hpp"
#include "move_picker.hpp"

struct SearchConfig
{
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
        this->maxDepth = std::clamp<i32>(newMaxDepth, 1, MAX_DEPTH);
    }

}; // struct SearchConfig

class Searcher
{
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
        // reset best move at root
        mainThreadData()->pliesData[0].pvLine.clear();

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

    inline std::pair<Move, i32> search(Position& pos, const SearchConfig& searchConfig)
    {
        blockUntilSleep();

        mSearchConfig = searchConfig;

        if (!hasLegalMove(pos))
        {
            ucinewgame();
            return { MOVE_NONE, pos.inCheck() ? -INF : 0 };
        }

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

        mStopSearch = false;

        for (ThreadData* td : mThreadsData)
        {
            td->pos = pos;
            td->score = std::nullopt;
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

        return { bestMoveAtRoot(), *(mainThreadData()->score) };
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
    }

    constexpr bool shouldStop(const ThreadData* td)
    {
        if (mStopSearch.load(std::memory_order_relaxed))
            return true;

        // Only check stop conditions and modify mStopSearch in main thread
        // Don't stop searching if depth 1 not completed
        if (td != mainThreadData() || !(mainThreadData()->score))
            return false;

        if (mSearchConfig.maxNodes < std::numeric_limits<u64>::max()
        &&  totalNodes() >= mSearchConfig.maxNodes)
            return mStopSearch = true;

        // Check time every N nodes
        if (td->nodes % 1024 != 0)
            return false;

        return mStopSearch = (millisecondsElapsed(mSearchConfig.startTime) >= mSearchConfig.hardMs);
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        search(td, 1, 0);

        // If main thread, signal other threads to stop searching
        if (td == mainThreadData())
            mStopSearch = true;
    }

    constexpr i32 evaluate(const Position& pos)
    {
        constexpr EnumArray<i32, PieceType> PIECE_VALUES = {
            100, 300, 300, 500, 900, 0
        };

        i32 eval = static_cast<i32>(pos.zobristHash() % 16);

        for (const PieceType pt : EnumIter<PieceType>())
        {
            const i32 numOurs   = std::popcount(pos.getBb(pos.sideToMove(),    pt));
            const i32 numTheirs = std::popcount(pos.getBb(pos.notSideToMove(), pt));
            eval += (numOurs - numTheirs) * PIECE_VALUES[pt];
        }

        return eval;
    }

    constexpr i32 search(ThreadData* td, i32 depth, i32 ply)
    {
        assert(hasLegalMove(td->pos));
        assert(ply >= 0 && ply <= MAX_DEPTH);
        //assert(alpha >= -INF && alpha <= INF);
        //assert(beta  >= -INF && beta  <= INF);
        //assert(alpha < beta);

        if (shouldStop(td)) return 0;

        // Quiescence search at leaf nodes
        if (depth <= 0) return evaluate(td->pos);

        // Max ply cutoff
        if (ply >= MAX_DEPTH) return evaluate(td->pos);

        depth = std::min<i32>(depth, MAX_DEPTH);

        [[maybe_unused]] size_t legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;

        MovePicker movePicker = MovePicker();
        Move move;

        while ((move = movePicker.nextLegal(td->pos)) != MOVE_NONE)
        {
            legalMovesSeen++;

            makeMove(td, move, static_cast<size_t>(ply + 1));

            const GameState gameState = td->pos.gameState(hasLegalMove, ply + 1);

            const i32 score = gameState == GameState::Draw ? 0
                            : gameState == GameState::Loss ? INF - (ply + 1)
                            : -search(td, depth - 1, ply + 1);

            undoMove(td);

            if (shouldStop(td)) return 0;

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;

                if (ply == 0) {
                    td->pliesData[0].pvLine.clear();
                    td->pliesData[0].pvLine.push_back(bestMove);
                }
            }
        }

        assert(legalMovesSeen > 0);

        return bestScore;
    }

}; // class Searcher
