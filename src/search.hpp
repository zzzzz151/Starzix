// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search_params.hpp"
#include "nnue.hpp"
#include "history_entry.hpp"
#include "thread_data.hpp"
#include "move_picker.hpp"
#include "tt.hpp"
#include "cuckoo.hpp"
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
            td->historyTable     = { };
            td->pawnsCorrHist    = { };
            td->nonPawnsCorrHist = { };
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

    inline Move search(Position& pos, const SearchConfig& searchConfig)
    {
        blockUntilSleep();

        mSearchConfig = searchConfig;

        // Init node counter of every thread to 1 (root node)
        for (ThreadData* td : mThreadsData)
            td->nodes.store(1, std::memory_order_relaxed);

        if (!hasLegalMove(pos))
            return MOVE_NONE;

        mStopSearch.store(false, std::memory_order_relaxed);

        // Init main thread's position
        mainThreadData()->pos = pos;

        // Init main thread's root accumulator
        nnue::BothAccumulators& bothAccs = mainThreadData()->bothAccsStack[0];
        bothAccs = nnue::BothAccumulators(pos);

        const auto initFinnyEntry = [&] (
            const Color color, const bool mirrorVAxis, const size_t inputBucket) constexpr
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
            td->pliesData[0] = PlyData();
            td->nodesByMove = { };
            td->bothAccsIdx = 0;

            wakeThread(td, ThreadState::Searching);
        }

        blockUntilSleep();

        return bestMoveAtRoot(mainThreadData());
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

    constexpr bool isHardTimeUp(const ThreadData* td)
    {
        if (td == mainThreadData()
        && td->rootDepth > 1
        && mSearchConfig.hardMs
        && td->nodes.load(std::memory_order_relaxed) % 1024 == 0
        && millisecondsElapsed(mSearchConfig.startTime) >= *(mSearchConfig.hardMs))
            mStopSearch.store(true, std::memory_order_relaxed);

        return mStopSearch.load(std::memory_order_relaxed);
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        const i32 maxDepth = static_cast<i32>(mSearchConfig.getMaxDepth());
        i32 score = 0;

        for (td->rootDepth = 1; td->rootDepth <= maxDepth; td->rootDepth++)
        {
            td->maxPlyReached = 0; // Reset seldepth

            score = td->rootDepth >= 4
                  ? aspirationWindows(td, score)
                  : search<true, true, false>(td, td->rootDepth, 0, -INF, INF);

            if (mStopSearch.load(std::memory_order_relaxed))
                break;

            // Only print uci info and check limits in main thread
            if (td != mainThreadData())
                continue;

            // Print uci info

            const u64 msElapsed = millisecondsElapsed(mSearchConfig.startTime);
            const u64 nodes = totalNodes();

            if (!mSearchConfig.printInfo)
                goto checkLimits;

            std::cout << "info"
                      << " depth "    << td->rootDepth
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

            checkLimits:

            // Hard time limit hit?
            if (mSearchConfig.hardMs && msElapsed >= *(mSearchConfig.hardMs))
                break;

            // Soft nodes limit hit?
            if (mSearchConfig.maxNodes && nodes >= *(mSearchConfig.maxNodes))
                break;

            // Nodes time management
            // Scale soft time limit based on nodes spent on best move at root
            const auto softMsScaled = [&] () constexpr -> u64
            {
                const u64 threadNodes   = td->nodes.load(std::memory_order_relaxed);
                const u64 bestMoveNodes = td->nodesByMove[bestMoveAtRoot(td).asU16()];

                const double bestMoveNodesFraction
                    = static_cast<double>(bestMoveNodes)
                    / static_cast<double>(std::max<u64>(threadNodes, 1));

                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);

                const double scaled
                    = static_cast<double>(*(mSearchConfig.softMs))
                    * (nodesTmBase() - bestMoveNodesFraction * nodesTmMul());

                return static_cast<u64>(round(scaled));
            };

            // Soft time limit hit?
            if (mSearchConfig.softMs
            && msElapsed >= (td->rootDepth >= 6 ? softMsScaled() : *(mSearchConfig.softMs)))
                break;
        }

        // If main thread, signal other threads to stop searching
        if (td == mainThreadData())
            mStopSearch.store(true, std::memory_order_relaxed);
    }

    constexpr i32 aspirationWindows(ThreadData* td, i32 score)
    {
        assert(td->rootDepth > 1);

        i32 depth = td->rootDepth;
        i32 delta = aspStartDelta();
        i32 alpha = std::max<i32>(score - delta, -INF);
        i32 beta  = std::min<i32>(score + delta, INF);

        while (true)
        {
            score = search<true, true, false>(td, depth, 0, alpha, beta);

            if (isHardTimeUp(td)) return 0;

            if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max<i32>(alpha - delta, -INF);
                depth = td->rootDepth;
            }
            else if (score >= beta)
            {
                beta = std::min<i32>(beta + delta, INF);
                if (depth > 1) depth--;
            }
            else
                return score;

            delta = static_cast<i32>(round(static_cast<double>(delta) * aspDeltaMul()));
        }
    }

    template<bool isRoot, bool isPvNode, bool isCutNode>
    constexpr i32 search(
        ThreadData* td,
        i32 depth,
        const size_t ply,
        i32 alpha,
        const i32 beta,
        const Move singularMove = MOVE_NONE)
    {
        assert(hasLegalMove(td->pos));
        assert(isRoot == (ply == 0));
        assert(ply >= 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);
        assert(!(isRoot && !isPvNode));
        assert(isPvNode || alpha + 1 == beta);
        assert(!(isPvNode && isCutNode));

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch<isPvNode>(td, ply, alpha, beta);

        if (isHardTimeUp(td)) return 0;

        if (!isRoot && alpha < 0 && td->pos.hasUpcomingRepetition(ply))
        {
            alpha = 0;

            if (alpha >= beta) return alpha;
        }

        depth = std::min<i32>(depth, static_cast<i32>(MAX_DEPTH));

        // Probe TT for TT entry
        TTEntry* ttEntryPtr = singularMove ? nullptr : &getEntry(mTT, td->pos.zobristHash());

        // Get TT entry data
        const auto [ttHit, ttDepth, ttScore, ttBound, ttMove]
            = ttEntryPtr == nullptr
            ? std::tuple{ false, 0, 0, Bound::None, MOVE_NONE }
            : ttEntryPtr->get(td->pos.zobristHash(), static_cast<i16>(ply));

        // TT cutoff
        if (!isPvNode
        && ttHit
        && ttDepth >= depth
        && (ttBound == Bound::Exact
        || (ttBound == Bound::Upper && ttScore <= alpha)
        || (ttBound == Bound::Lower && ttScore >= beta)))
            return ttScore;

        const auto eval = [&] () constexpr {
            return getEval(td, ply);
        };

        if (ply >= MAX_DEPTH) return eval();

        // Reset killer move of next tree level
        td->pliesData[ply + 1].killer = MOVE_NONE;

        updateBothAccs(td);

        // Node pruning
        if (!isPvNode && !td->pos.inCheck() && !singularMove)
        {
            // RFP (Reverse futility pruning)
            if (depth <= 7 && eval() >= beta + depth * rfpDepthMul())
                return eval();

            // NMP (Null move pruning)
            if (depth >= 3
            && td->pos.lastMove()
            && td->pos.stmHasNonPawns()
            && !(ttHit && ttBound == Bound::Upper && ttScore < beta)
            && eval() >= beta)
            {
                const GameState newGameState = makeMove(td, MOVE_NONE, ply + 1);

                const i32 nmpDepth = depth - 3 - depth / 3;

                const i32 score
                    = newGameState == GameState::Draw
                    ? 0
                    : newGameState == GameState::Loss
                    ? INF - static_cast<i32>(ply + 1)
                    : -search<false, false, false>(td, nmpDepth, ply + 1, -beta, -alpha);

                undoMove(td);

                if (mStopSearch.load(std::memory_order_relaxed))
                    return 0;

                if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }
        }

        // IIR (Internal iterative reduction)
        if (depth >= 4 && !ttMove && !singularMove)
            depth--;

        PlyData& plyData = td->pliesData[ply];

        size_t legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::Upper;

        ArrayVec<Move, 256> failLowQuiets;
        ArrayVec<std::pair<Move, std::optional<PieceType>>, 256> failLowNoisies; // move, captured

        // Moves loop

        MovePicker movePicker = MovePicker(false, ttMove, plyData.killer, singularMove);

        while (true)
        {
            // Move, std::optional<i32>
            const auto [move, moveScore] = movePicker.nextLegal(td->pos, td->historyTable);

            if (!move) break;

            assert(move != singularMove);

            legalMovesSeen++;
            const bool isQuiet = td->pos.isQuiet(move);

            if (bestScore > -MIN_MATE_SCORE && (isQuiet || *moveScore < 0))
            {
                // LMP (Late move pruning)
                if (!isPvNode
                && !td->pos.inCheck()
                && legalMovesSeen > static_cast<size_t>(3 + depth * depth * 3 / 4))
                    break;

                const i32 lmrDepth
                    = depth - LMR_TABLE[static_cast<size_t>(depth)][isQuiet][legalMovesSeen];

                // FP (Futility pruning)
                if (!isPvNode
                && !td->pos.inCheck()
                && lmrDepth <= 6
                && legalMovesSeen > 2
                && alpha < MIN_MATE_SCORE
                && alpha > eval() + fpBase() + std::max<i32>(lmrDepth, 1) * fpDepthMul())
                    break;

                // SEE pruning

                const i32 threshold
                    = depth * (isQuiet ? seeQuietThreshold() : seeNoisyThreshold());

                if (!isRoot && td->pos.stmHasNonPawns() && !td->pos.SEE(move, threshold))
                    continue;
            }

            i32 newDepth = depth - 1;

            // SE (Singular extensions)
            // In singular searches, ttMove = MOVE_NONE, which prevents SE
            if (!isRoot
            && move == ttMove
            && static_cast<i32>(ply) < td->rootDepth * 2
            && depth >= 8
            && ttDepth >= depth - 3
            && std::abs(ttScore) < MIN_MATE_SCORE
            && ttBound == Bound::Lower)
            {
                const i32 singularBeta = ttScore - depth;

                const i32 singularScore = search<isRoot, false, isCutNode>(
                    td, newDepth / 2, ply, singularBeta - 1, singularBeta, ttMove
                );

                // Single or double extension
                if (singularScore < singularBeta)
                    newDepth += 1 + (singularScore < singularBeta - doubleExtMargin());
                // Multicut
                else if (singularScore >= beta && std::abs(singularScore) < MIN_MATE_SCORE)
                    return singularScore;
            }

            HistoryEntry& histEntry
                = td->historyTable[td->pos.sideToMove()][move.pieceType()][move.to()];

            const u64 nodesBefore = td->nodes.load(std::memory_order_relaxed);

            const GameState newGameState = makeMove(td, move, ply + 1);

            const std::optional<PieceType> captured = td->pos.captured();

            i32 score = 0;

            if (newGameState != GameState::Ongoing)
            {
                score = newGameState == GameState::Draw ? 0 : INF - static_cast<i32>(ply + 1);
                goto moveSearched;
            }

            // PVS (Principal variation search)

            // LMR (Late move reductions)
            if (depth >= 2 && legalMovesSeen > 1 + isRoot && (isQuiet || *moveScore < 0))
            {
                i32 lmr = LMR_TABLE[static_cast<size_t>(depth)][isQuiet][legalMovesSeen]
                        - isPvNode
                        - td->pos.inCheck()
                        + isCutNode * 2;

                if (isQuiet) {
                    const PosState posState = td->pos.getState();
                    td->pos.undoMove();

                    const auto history = static_cast<float>(histEntry.quietHistory(td->pos, move));
                    lmr -= static_cast<i32>(round(history * lmrQuietHistoryMul()));

                    td->pos.pushState(posState);
                }

                lmr = std::max<i32>(lmr, 0); // Don't extend depth

                score = -search<false, false, true>(
                    td, newDepth - lmr, ply + 1, -alpha - 1, -alpha
                );

                if (score > alpha && lmr > 0)
                {
                    score = -search<false, false, !isCutNode>(
                        td, newDepth, ply + 1, -alpha - 1, -alpha
                    );
                }
            }
            else if (!isPvNode || legalMovesSeen > 1)
            {
                score = -search<false, false, !isCutNode>(
                    td, newDepth, ply + 1, -alpha - 1, -alpha
                );
            }

            if (isPvNode && (legalMovesSeen == 1 || score > alpha))
                score = -search<false, true, false>(td, newDepth, ply + 1, -beta, -alpha);

            moveSearched:

            undoMove(td);

            if (mStopSearch.load(std::memory_order_relaxed))
                return 0;

            assert(td->nodes.load(std::memory_order_relaxed) > nodesBefore);

            if constexpr (isRoot)
            {
                td->nodesByMove[move.asU16()]
                    += td->nodes.load(std::memory_order_relaxed) - nodesBefore;
            }

            bestScore = std::max<i32>(bestScore, score);

            // Fail low?
            if (score <= alpha)
            {
                if (td->pos.isQuiet(move))
                    failLowQuiets.push_back(move);
                else
                    failLowNoisies.push_back({ move, captured });

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

            // We have a fail high

            bound = Bound::Lower;

            // Update histories

            const i32 histBonus = std::clamp<i32>(
                depth * historyBonusMul() - historyBonusOffset(), 0, historyBonusMax()
            );

            const i32 histMalus = -std::clamp<i32>(
                depth * historyMalusMul() - historyMalusOffset(), 0, historyMalusMax()
            );

            // Decrease history of fail low noisy moves
            for (const auto [mov, optCaptured] : failLowNoisies)
            {
                td->historyTable[td->pos.sideToMove()][mov.pieceType()][mov.to()]
                    .updateNoisyHistory(optCaptured, mov.promotion(), histMalus);
            }

            if (!isQuiet) {
                // Increase history of this fail high noisy move
                histEntry.updateNoisyHistory(captured, move.promotion(), histBonus);
                break;
            }

            // At this point, this move is quiet

            plyData.killer = move;

            // Increase quiet histories of this fail high quiet move
            histEntry.updateQuietHistories(td->pos, move, histBonus);

            // Decrease quiet histories of fail low quiet moves
            for (const Move m : failLowQuiets)
            {
                td->historyTable[td->pos.sideToMove()][m.pieceType()][m.to()]
                    .updateQuietHistories(td->pos, m, histMalus);
            }

            break;
        }

        if (legalMovesSeen == 0)
        {
            assert(singularMove);
            return alpha;
        }

        if (!singularMove)
        {
            // Update TT entry
            ttEntryPtr->update(
                td->pos.zobristHash(),
                static_cast<u8>(depth),
                static_cast<i16>(bestScore),
                static_cast<i16>(ply),
                bound,
                bestMove
            );

            // Update correction histories
            if (!td->pos.inCheck()
            && std::abs(bestScore) < MIN_MATE_SCORE
            && (!bestMove || td->pos.isQuiet(bestMove))
            && !(bound == Bound::Lower && bestScore <= eval())
            && !(bound == Bound::Upper && bestScore >= eval()))
            {
                const i32 bonus = (bestScore - eval()) * depth;

                for (i16* corrHistPtr : getCorrHists(td))
                    updateHistory(corrHistPtr, bonus);
            }
        }

        assert(std::abs(bestScore) < INF);
        return bestScore;
    }

    // Quiescence search
    template<bool isPvNode>
    constexpr i32 qSearch(ThreadData* td, const size_t ply, i32 alpha, const i32 beta)
    {
        assert(hasLegalMove(td->pos));
        assert(ply > 0 && ply < MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);
        assert(isPvNode || alpha + 1 == beta);

        if (isHardTimeUp(td)) return 0;

        if (alpha < 0 && td->pos.hasUpcomingRepetition(ply))
        {
            alpha = 0;

            if (alpha >= beta) return alpha;
        }

        const i32 eval = getEval(td, ply);

        if (ply >= MAX_DEPTH) return eval;

        if (!td->pos.inCheck())
        {
            // Stand pat
            // Return immediately if static eval fails high
            if (eval >= beta) return eval;

            alpha = std::max<i32>(alpha, eval);
        }
        else
            updateBothAccs(td);

        // Reset killer move of next tree level
        td->pliesData[ply + 1].killer = MOVE_NONE;

        i32 bestScore = td->pos.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;

        // Moves loop

        MovePicker movePicker = MovePicker(
            !td->pos.inCheck(), MOVE_NONE, td->pliesData[ply].killer
        );

        while (true)
        {
            // Move, std::optional<i32>
            const auto [move, moveScore] = movePicker.nextLegal(td->pos, td->historyTable);

            if (!move) break;

            // Prune underpromotions
            if (bestScore > -MIN_MATE_SCORE && move.isUnderpromotion())
                break;

            // SEE pruning
            if (!td->pos.inCheck()
            &&  !td->pos.SEE(move, 0 /* getSEEThreshold(td->pos, td->historyTable, move) */ ))
                continue;

            const GameState newGameState = makeMove(td, move, ply + 1);

            const i32 score
                = newGameState == GameState::Draw
                ? 0
                : newGameState == GameState::Loss
                ? INF - static_cast<i32>(ply + 1)
                : -qSearch<isPvNode>(td, ply + 1, -beta, -alpha);

            undoMove(td);

            if (mStopSearch.load(std::memory_order_relaxed))
                return 0;

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
