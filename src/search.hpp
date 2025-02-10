// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "nnue.hpp"
#include "search_params.hpp"
#include "history_entry.hpp"
#include "thread_data.hpp"
#include "move_picker.hpp"
#include "tt.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

struct SearchConfig
{
private:

    i32 maxDepth = MAX_DEPTH;

public:

    std::optional<u64> maxNodes = std::nullopt;

    std::chrono::time_point<std::chrono::steady_clock> startTime
        = std::chrono::steady_clock::now();

    std::optional<u64> hardMs = std::nullopt;
    std::optional<u64> softMs = std::nullopt;

    bool printInfo = true;

    constexpr auto getMaxDepth() const {
        return this->maxDepth;
    }

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

    std::vector<TTEntry> mTT = { }; // Transposition table

    inline Searcher() {
        setThreads(1);
        resizeTT(mTT, 32); // Default TT size is 32 MiB
    }

    inline ~Searcher() {
        setThreads(0);
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

    constexpr void ucinewgame()
    {
        for (ThreadData* td : mThreadsData)
        {
            td->nodes.store(0, std::memory_order_relaxed);
            td->historyTable = { };
        }

        resetTT(mTT);
    }

    constexpr u64 totalNodes() const
    {
        u64 nodes = 0;

        for (const ThreadData* td : mThreadsData)
            nodes += td->nodes.load(std::memory_order_relaxed);

        return nodes;
    }

    inline std::pair<Move, i32> search(Position& pos, const SearchConfig& searchConfig)
    {
        blockUntilSleep();

        mSearchConfig = searchConfig;

        if (mSearchConfig.maxNodes && *(mSearchConfig.maxNodes) < std::numeric_limits<u64>::max())
            *(mSearchConfig.maxNodes) += 1;

        // Init node counter of every thread to 1 (root node)
        for (ThreadData* td : mThreadsData)
            td->nodes.store(1, std::memory_order_relaxed);

        if (!hasLegalMove(pos))
            return { MOVE_NONE, pos.inCheck() ? -INF : 0 };

        mStopSearch.store(false, std::memory_order_relaxed);

        // Init main thread's position
        mainThreadData()->pos = pos;

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
                finnyEntry.colorBbs  = pos.colorBbs();
                finnyEntry.piecesBbs = pos.piecesBbs();
            }
            else {
                finnyEntry.accumulator = nnue::NET->hiddenBiases[color];
                finnyEntry.colorBbs  = { };
                finnyEntry.piecesBbs = { };
            }
        };

        // Init main thread's finny table
        for (const Color color : EnumIter<Color>())
            for (const bool mirrorVAxis : { false, true })
                for (size_t inputBucket = 0; inputBucket < nnue::NUM_INPUT_BUCKETS; inputBucket++)
                    initFinnyEntry(color, mirrorVAxis, inputBucket);

        // Init position, root accumulator and finny table of secondary threads
        for (size_t i = 1; i < mThreadsData.size(); i++)
        {
            mThreadsData[i]->pos = pos;
            mThreadsData[i]->bothAccsStack[0] = bothAccs;
            mThreadsData[i]->finnyTable = mainThreadData()->finnyTable;
        }

        for (ThreadData* td : mThreadsData)
        {
            td->score = std::nullopt;
            td->pliesData[0] = PlyData();
            td->nodesByMove = { };
            td->bothAccsIdx = 0;

            wakeThread(td, ThreadState::Searching);
        }

        blockUntilSleep();

        return { bestMoveAtRoot(mainThreadData()), *(mainThreadData()->score) };
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

    constexpr bool shouldStop(const ThreadData* td)
    {
        if (mStopSearch.load(std::memory_order_relaxed))
            return true;

        // Only check stop conditions and modify mStopSearch in main thread
        // Don't stop searching if depth 1 not completed
        if (td != mainThreadData() || !(mainThreadData()->score))
            return false;

        if (mSearchConfig.maxNodes && totalNodes() >= *(mSearchConfig.maxNodes))
        {
            mStopSearch.store(true, std::memory_order_relaxed);
            return true;
        }

        // Check time every N nodes
        if (!mSearchConfig.hardMs || td->nodes.load(std::memory_order_relaxed) % 1024 != 0)
            return false;

        if (millisecondsElapsed(mSearchConfig.startTime) >= *(mSearchConfig.hardMs))
        {
            mStopSearch.store(true, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        for (i32 depth = 1; depth <= mSearchConfig.getMaxDepth(); depth++)
        {
            td->maxPlyReached = 0; // Reset seldepth

            const i32 score = search<true>(td, depth, 0, -INF, INF);

            if (mStopSearch.load(std::memory_order_relaxed))
                break;

            td->score = score;

            // If not main thread, continue
            if (td != mainThreadData())
                continue;

            const u64 msElapsed = millisecondsElapsed(mSearchConfig.startTime);
            const u64 nodes = totalNodes();

            const bool hardMsHit = mSearchConfig.hardMs && msElapsed >= *(mSearchConfig.hardMs);
            const bool maxNodesHit = mSearchConfig.maxNodes && nodes >= *(mSearchConfig.maxNodes);

            if (hardMsHit || maxNodesHit)
                mStopSearch.store(true, std::memory_order_relaxed);

            if (!mSearchConfig.printInfo)
                goto checkSoftTime;

            std::cout << "info"
                      << " depth "    << depth
                      << " seldepth " << td->maxPlyReached
                      << " score ";

            if (std::abs(score) < MIN_MATE_SCORE)
                std::cout << "cp " << score;
            else {
                const i32 pliesToMate = INF - std::abs(score);
                const i32 fullMovesToMate = (pliesToMate + 1) / 2;
                std::cout << "mate " << (score > 0 ? fullMovesToMate : -fullMovesToMate);
            }

            std::cout << " nodes " << nodes
                      << " nps "   << getNps(nodes, msElapsed)
                      << " time "  << msElapsed
                      << " pv";

            for (const Move move : td->pliesData[0].pvLine)
                std::cout << " " << move.toUci();

            std::cout << std::endl;

            checkSoftTime:

            if (depth >= 6 && mSearchConfig.softMs && msElapsed >= *(mSearchConfig.softMs))
                break;
        }

        // If main thread, signal other threads to stop searching
        if (td == mainThreadData())
            mStopSearch.store(true, std::memory_order_relaxed);
    }

    template<bool isRoot>
    constexpr i32 search(ThreadData* td, i32 depth, const i32 ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(isRoot == (ply == 0));
        assert(ply >= 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch(td, ply, alpha, beta);

        if (shouldStop(td)) return 0;

        depth = std::min<i32>(depth, MAX_DEPTH);

        nnue::BothAccumulators& bothAccs = td->bothAccsStack[td->bothAccsIdx];

        if (!bothAccs.mUpdated)
        {
            assert(td->bothAccsIdx > 0);
            bothAccs.update(td->bothAccsStack[td->bothAccsIdx - 1], td->pos, td->finnyTable);
        }

        assert(bothAccs == nnue::BothAccumulators(td->pos));

        // Probe TT for TT entry
        TTEntry& ttEntryRef = getEntry(mTT, td->pos.zobristHash());

        // No TT entry if collision
        std::optional<TTEntry> ttEntry = ttEntryRef.zobristHash == td->pos.zobristHash()
                                       ? std::optional<TTEntry> { ttEntryRef }
                                       : std::nullopt;

        Move ttMove = MOVE_NONE;

        if (ttEntry) {
            ttEntry->adjustScore(static_cast<i16>(ply));
            ttMove = Move(ttEntry->move);

            // TT cutoff
            if (!isRoot
            && ttEntry->depth >= depth
            && (ttEntry->bound == Bound::Exact
            || (ttEntry->bound == Bound::Upper && ttEntry->score <= alpha)
            || (ttEntry->bound == Bound::Lower && ttEntry->score >= beta)))
                return ttEntry->score;
        }

        [[maybe_unused]] size_t legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::Upper;

        MovePicker movePicker = MovePicker(false, ttMove);
        Move move;

        while ((move = movePicker.nextLegal(td->pos, td->historyTable)) != MOVE_NONE)
        {
            legalMovesSeen++;

            const std::optional<i32> optScore = makeMove(td, move, ply + 1);

            const i32 score = optScore
                            ? -(*optScore)
                            : -search<false>(td, depth - 1, ply + 1, -beta, -alpha);

            undoMove(td);

            if (shouldStop(td)) return 0;

            bestScore = std::max<i32>(bestScore, score);

            // Fail low?
            if (score <= alpha) continue;

            alpha = score;
            bestMove = move;
            bound = Bound::Exact;

            if (isRoot) {
                td->pliesData[0].pvLine.clear();
                td->pliesData[0].pvLine.push_back(bestMove);
            }

            if (score < beta) continue;

            // Fail high

            bound = Bound::Lower;

            if (td->pos.isQuiet(move))
            {
                const i32 histBonus = std::clamp<i32>(
                    depth * historyBonusMul() - historyBonusOffset(), 0, historyBonusMax()
                );

                td->historyTable[td->pos.sideToMove()][move.pieceType()][move.to()]
                    .update(histBonus);
            }

            break;
        }

        assert(legalMovesSeen > 0);

        // Update TT entry
        ttEntryRef.update(
            td->pos.zobristHash(),
            static_cast<u8>(depth),
            static_cast<i16>(bestScore),
            static_cast<i16>(ply),
            bound,
            bestMove
        );

        assert(std::abs(bestScore) < INF);
        return bestScore;
    }

    // Quiescence search
    constexpr i32 qSearch(ThreadData* td, const i32 ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(ply > 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        const std::optional<i32> eval = updateBothAccsAndEval(td, ply);

        if (!td->pos.inCheck())
        {
            if (*eval >= beta) return *eval;

            alpha = std::max<i32>(alpha, *eval);
        }

        i32 bestScore = td->pos.inCheck() ? -INF : *eval;
        Move bestMove = MOVE_NONE;

        MovePicker movePicker = MovePicker(!td->pos.inCheck(), MOVE_NONE);
        Move move;

        while ((move = movePicker.nextLegal(td->pos, td->historyTable)) != MOVE_NONE)
        {
            if (bestScore > -MIN_MATE_SCORE && move.isUnderpromotion())
                break;

            const std::optional<i32> optScore = makeMove(td, move, ply + 1);

            const i32 score = optScore
                            ? -(*optScore)
                            : -qSearch(td, ply + 1, -beta, -alpha);

            undoMove(td);

            if (shouldStop(td)) return 0;

            bestScore = std::max<i32>(bestScore, score);

            if (score > alpha)
            {
                alpha = score;
                bestMove = move;
            }

             // Fail high?
            if (score >= beta) break;
        }

        assert(std::abs(bestScore) < INF);
        return bestScore;
    }

}; // class Searcher
