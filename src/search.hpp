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

    size_t maxDepth = MAX_DEPTH;

public:

    std::optional<u64> maxNodes = std::nullopt;

    std::chrono::time_point<std::chrono::steady_clock> startTime
        = std::chrono::steady_clock::now();

    std::optional<u64> hardMs = std::nullopt;
    std::optional<u64> softMs = std::nullopt;

    bool printInfo = true;

    constexpr auto getMaxDepth() const
    {
        return this->maxDepth;
    }

    constexpr void setMaxDepth(const i32 newMaxDepth)
    {
        this->maxDepth = static_cast<size_t>(std::clamp<i32>(
            newMaxDepth, 1, static_cast<i32>(MAX_DEPTH)
        ));
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

        // Hit nodes limit?
        if (mSearchConfig.maxNodes && totalNodes() >= *(mSearchConfig.maxNodes))
        {
            mStopSearch.store(true, std::memory_order_relaxed);
            return true;
        }

        // Check hard time limit every N nodes
        if (!mSearchConfig.hardMs || td->nodes.load(std::memory_order_relaxed) % 1024 != 0)
            return false;

        // Hit hard time limit?
        if (millisecondsElapsed(mSearchConfig.startTime) >= *(mSearchConfig.hardMs))
        {
            mStopSearch.store(true, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        for (i32 depth = 1; depth <= static_cast<i32>(mSearchConfig.getMaxDepth()); depth++)
        {
            td->maxPlyReached = 0; // Reset seldepth

            const i32 score = search<true, true>(td, depth, 0, -INF, INF);

            if (mStopSearch.load(std::memory_order_relaxed))
                break;

            td->score = score;

            // Only print uci info and check soft time limit in main thread
            if (td != mainThreadData())
                continue;

            // Print uci info

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

            // Stop searching if soft time limit has been hit

            if (mSearchConfig.softMs && msElapsed >= *(mSearchConfig.softMs))
                break;
        }

        // If main thread, signal other threads to stop searching
        if (td == mainThreadData())
            mStopSearch.store(true, std::memory_order_relaxed);
    }

    template<bool isRoot, bool isPvNode>
    constexpr i32 search(ThreadData* td, i32 depth, const size_t ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(isRoot == (ply == 0));
        assert(ply >= 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);
        assert(!(isRoot && !isPvNode));
        assert(isPvNode || alpha + 1 == beta);

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch(td, ply, alpha, beta);

        if (shouldStop(td)) return 0;

        depth = std::min<i32>(depth, static_cast<i32>(MAX_DEPTH));

        // Probe TT for TT entry
        TTEntry& ttEntryRef = getEntry(mTT, td->pos.zobristHash());

        // Parse TT entry
        const std::optional<ParsedTTEntry> ttEntry
            = ParsedTTEntry::parse(ttEntryRef, td->pos.zobristHash(), static_cast<i16>(ply));

        // TT cutoff
        if (ttEntry
        && !isPvNode
        && ttEntry->depth >= depth
        && (ttEntry->bound == Bound::Exact
        || (ttEntry->bound == Bound::Upper && ttEntry->score <= alpha)
        || (ttEntry->bound == Bound::Lower && ttEntry->score >= beta)))
            return ttEntry->score;

        updateBothAccs(td);

        const auto eval = [&] () constexpr -> i32 {
            return getEval(td, ply);
        };

        if (!isPvNode && !td->pos.inCheck())
        {
            // NMP (Null move pruning)
            if (depth >= 3
            && td->pos.lastMove() != MOVE_NONE
            && td->pos.stmHasNonPawns()
            && !(ttEntry && ttEntry->bound == Bound::Upper && ttEntry->score < beta)
            && eval() >= beta)
            {
                const std::optional<i32> optScore = makeMove(td, MOVE_NONE, ply + 1);

                const i32 score = optScore
                                ? -(*optScore)
                                : -search<false, false>(td, depth - 3 - depth / 3, ply + 1, -beta, -alpha);

                undoMove(td);

                if (shouldStop(td)) return 0;

                if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }
        }

        PlyData& plyData = td->pliesData[ply];

        // Reset killer move of next tree level
        td->pliesData[ply + 1].killer = MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= 4 && (!ttEntry || ttEntry->move == MOVE_NONE))
            depth--;

        [[maybe_unused]] size_t legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::Upper;

        ArrayVec<Move, 256> failLowQuiets;

        // Moves loop

        MovePicker movePicker = MovePicker(
            false, ttEntry ? ttEntry->move : MOVE_NONE, plyData.killer
        );

        while (true)
        {
            // moveRanking is a std::optional<MoveRanking>
            const auto [move, moveRanking] = movePicker.nextLegal(td->pos, td->historyTable);

            if (move == MOVE_NONE) break;

            legalMovesSeen++;
            const bool isQuiet = td->pos.isQuiet(move);

            if (!isRoot
            && bestScore > -MIN_MATE_SCORE
            && static_cast<i32>(*moveRanking) < static_cast<i32>(MoveRanking::GoodNoisy)
            && td->pos.stmHasNonPawns())
            {
                // SEE pruning
                const i32 threshold = depth * (isQuiet ? seeQuietThreshold() : seeNoisyThreshold());
                if (!td->pos.SEE(move, threshold))
                    continue;
            }

            const std::optional<i32> optScore = makeMove(td, move, ply + 1);

            i32 score = 0;

            if (optScore) {
                score = -(*optScore);
                goto moveSearched;
            }

            if (!isPvNode || legalMovesSeen > 1)
                score = -search<false, false>(td, depth - 1, ply + 1, -alpha - 1, -alpha);

            if (isPvNode && (legalMovesSeen == 1 || score > alpha))
                score = -search<false, true>(td, depth - 1, ply + 1, -beta, -alpha);

            moveSearched:

            undoMove(td);

            if (shouldStop(td)) return 0;

            bestScore = std::max<i32>(bestScore, score);

            // Fail low?
            if (score <= alpha)
            {
                if (td->pos.isQuiet(move))
                    failLowQuiets.push_back(move);

                continue;
            }

            alpha = score;
            bestMove = move;
            bound = Bound::Exact;

            // In PV nodes, update PV line
            if constexpr (isPvNode)
            {
                plyData.pvLine.clear();
                plyData.pvLine.push_back(move);

                // Copy child's PV line
                for (const Move m : td->pliesData[ply + 1].pvLine)
                    plyData.pvLine.push_back(m);
            }

            if (score < beta) continue;

            // Fail high

            bound = Bound::Lower;

            if (!isQuiet) break;

            // We have a quiet move

            plyData.killer = move;

            // Increase move's history

            const i32 histBonus = std::clamp<i32>(
                depth * historyBonusMul() - historyBonusOffset(), 0, historyBonusMax()
            );

            td->historyTable[td->pos.sideToMove()][move.pieceType()][move.to()]
                .update(histBonus);

            // Decrease history of fail low quiets

            const i32 histMalus = -std::clamp<i32>(
                depth * historyMalusMul() - historyMalusOffset(), 0, historyMalusMax()
            );

            for (const Move failLowQuiet : failLowQuiets)
            {
                td->historyTable[td->pos.sideToMove()][failLowQuiet.pieceType()][failLowQuiet.to()]
                    .update(histMalus);
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
    constexpr i32 qSearch(ThreadData* td, const size_t ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(ply > 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        updateBothAccs(td);

        const i32 eval = getEval(td, ply);

        if (!td->pos.inCheck())
        {
            // Stand pat
            // Return immediately if static eval fails high
            if (eval >= beta) return eval;

            alpha = std::max<i32>(alpha, eval);
        }

        // Reset killer move of next tree level
        td->pliesData[ply + 1].killer = MOVE_NONE;

        i32 bestScore = td->pos.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;

        // Moves loop

        const Move killer = td->pliesData[ply].killer;
        MovePicker movePicker = MovePicker(!td->pos.inCheck(), MOVE_NONE, killer);

        while (true)
        {
            // moveRanking is a std::optional<MoveRanking>
            const auto [move, moveRanking] = movePicker.nextLegal(td->pos, td->historyTable);

            if (move == MOVE_NONE) break;

            // Prune underpromotions
            if (bestScore > -MIN_MATE_SCORE && move.isUnderpromotion())
                break;

            // SEE pruning
            if (!td->pos.inCheck() && !td->pos.SEE(move))
                continue;

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
