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

    std::optional<u64> maxNodes = std::nullopt;
    std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
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

    inline Searcher() {
        setThreads(1);
    }

    inline ~Searcher() {
        setThreads(0);
    }

    constexpr void ucinewgame()
    {
        for (ThreadData* td : mThreadsData)
            td->nodes = 0;
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

        if (mSearchConfig.maxNodes && *(mSearchConfig.maxNodes) < std::numeric_limits<u64>::max())
            *(mSearchConfig.maxNodes) += 1;

        // Initialize node counter of every thread to 1 (root node)
        for (ThreadData* td : mThreadsData)
            td->nodes = 1;

        if (!hasLegalMove(pos))
            return { MOVE_NONE, pos.inCheck() ? -INF : 0 };

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

        if (mSearchConfig.maxNodes && totalNodes() >= *(mSearchConfig.maxNodes))
            return mStopSearch = true;

        // Check time every N nodes
        if (!mSearchConfig.hardMs || td->nodes % 1024 != 0)
            return false;

        return mStopSearch = (millisecondsElapsed(mSearchConfig.startTime) >= *(mSearchConfig.hardMs));
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        for (i32 depth = 1; depth <= mSearchConfig.getMaxDepth(); depth++)
        {
            const Move bestMoveAtRootBefore = bestMoveAtRoot(td);

            td->maxPlyReached = 0; // Reset seldepth

            const i32 score = search(td, depth, 0, -INF, INF);

            if (mStopSearch.load(std::memory_order_relaxed))
            {
                td->pliesData[0].pvLine.clear();
                td->pliesData[0].pvLine.push_back(bestMoveAtRootBefore);
                break;
            }

            td->score = score;

            // If not main thread, continue
            if (td != mainThreadData())
                continue;

            const u64 msElapsed = millisecondsElapsed(mSearchConfig.startTime);
            const u64 nodes = totalNodes();

            mStopSearch = (mSearchConfig.hardMs && msElapsed >= *(mSearchConfig.hardMs))
                       || (mSearchConfig.maxNodes && nodes >= *(mSearchConfig.maxNodes));

            if (!mSearchConfig.printInfo)
                continue;

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
        }

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

    constexpr i32 search(ThreadData* td, i32 depth, const i32 ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(ply >= 0 && ply <= MAX_DEPTH);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        // Quiescence search at leaf nodes
        if (depth <= 0) return evaluate(td->pos);

        if (shouldStop(td)) return 0;

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
                            : -search(td, depth - 1, ply + 1, -beta, -alpha);

            undoMove(td);

            if (shouldStop(td)) return 0;

            bestScore = std::max<i32>(bestScore, score);

            // Fail low?
            if (score <= alpha) continue;

            alpha = score;
            bestMove = move;

            if (ply == 0) {
                td->pliesData[0].pvLine.clear();
                td->pliesData[0].pvLine.push_back(bestMove);
            }

            if (score < beta) continue;

            // Fail high

            break;
        }

        assert(legalMovesSeen > 0);

        return bestScore;
    }

    constexpr i32 qSearch(ThreadData* td, const i32 ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(ply > 0 && ply <= MAX_DEPTH);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        const i32 eval = evaluate(td->pos);

        // Max ply cutoff
        if (ply >= MAX_DEPTH) return eval;

        if (eval >= beta) return eval;

        alpha = std::max<i32>(alpha, eval);

        i32 bestScore = eval;
        Move bestMove = MOVE_NONE;

        MovePicker movePicker = MovePicker();
        Move move;

        while ((move = movePicker.nextLegalNoisy(td->pos, false)) != MOVE_NONE)
        {
            makeMove(td, move, static_cast<size_t>(ply + 1));

            const GameState gameState = td->pos.gameState(hasLegalMove, ply + 1);

            const i32 score = gameState == GameState::Draw ? 0
                            : gameState == GameState::Loss ? INF - (ply + 1)
                            : -qSearch(td, ply + 1, -beta, -alpha);

            undoMove(td);

            if (shouldStop(td)) return 0;

            if (score <= bestScore) continue;

            bestScore = score;

            if (bestScore > alpha) {
                alpha = bestScore;
                bestMove = move;
            }

             // Fail high?
            if (bestScore >= beta) break;
        }

        return bestScore;
    }

}; // class Searcher
