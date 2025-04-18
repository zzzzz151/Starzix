// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search_params.hpp"
#include "nnue.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "thread_data.hpp"
#include "move_picker.hpp"
#include "cuckoo.hpp"
#include <atomic>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>

struct SearchConfig
{
public:

    i32 maxDepth = MAX_DEPTH;

    std::optional<u64> maxNodes = std::nullopt;

    std::chrono::time_point<std::chrono::steady_clock> startTime
        = std::chrono::steady_clock::now();

    std::optional<u64> hardMs = std::nullopt;
    std::optional<u64> softMs = std::nullopt;

    bool printInfo = true;

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

        std::memset(mTT.data(), 0, mTT.size() * sizeof(TTEntry));
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

        mSearchConfig.maxDepth = std::clamp<i32>(mSearchConfig.maxDepth, 1, MAX_DEPTH);

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

        mStopSearch.store(false, std::memory_order_relaxed);

        for (ThreadData* td : mThreadsData)
        {
            td->pliesData[0] = { };
            td->pliesData[0].inCheck = td->pos.inCheck();
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
        // Only check time in main thread and if depth 1 completed
        if (td == mainThreadData()
        && td->rootDepth > 1
        && mSearchConfig.hardMs.has_value()
        && td->nodes.load(std::memory_order_relaxed) % 1024 == 0
        && millisecondsElapsed(mSearchConfig.startTime) >= mSearchConfig.hardMs)
            mStopSearch.store(true, std::memory_order_relaxed);

        return mStopSearch.load(std::memory_order_relaxed);
    }

    constexpr void iterativeDeepening(ThreadData* td)
    {
        i32 score    = 0;
        i32 avgScore = 0;

        size_t stableScoreStreak = 0;
        size_t bestMoveStreak    = 0;

        for (td->rootDepth = 1; td->rootDepth <= mSearchConfig.maxDepth; td->rootDepth++)
        {
            const Move prevBestMove = bestMoveAtRoot(td);

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
            if (mSearchConfig.hardMs.has_value() && msElapsed >= mSearchConfig.hardMs)
                break;

            // Soft nodes limit hit?
            if (mSearchConfig.maxNodes.has_value() && nodes >= mSearchConfig.maxNodes)
                break;

            // Soft time limit hit?

            if (!mSearchConfig.softMs.has_value())
                continue;

            // Keep track of score stability for soft time scaling
            // Deeper search scores are valued more
            if (td->rootDepth <= 1)
                avgScore = score;
            else {
                avgScore = (score + avgScore) / 2;
                stableScoreStreak = std::abs(score - avgScore) <= 10 ? stableScoreStreak + 1 : 0;
            }

            // Keep track of best move stability for soft time scaling
            bestMoveStreak = bestMoveAtRoot(td) == prevBestMove ? bestMoveStreak + 1 : 0;

            const auto softMsScaled = [&] () constexpr -> u64
            {
                // Nodes time management
                // Less/more soft time the bigger/smaller the fraction of nodes spent on best move

                const u64 threadNodes   = td->nodes.load(std::memory_order_relaxed);
                const u64 bestMoveNodes = td->nodesByMove[bestMoveAtRoot(td).asU16()];

                const double bestMoveNodesFraction
                    = static_cast<double>(bestMoveNodes)
                    / static_cast<double>(std::max<u64>(threadNodes, 1));

                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);

                double scale = tmNodesBase() - bestMoveNodesFraction * tmNodesMul();

                // Score stability time management
                // Less/more soft time the more/less stable the root score is
                scale *= std::max<double>(
                    tmScoreStabBase() - static_cast<double>(stableScoreStreak) * tmScoreStabMul(),
                    tmScoreStabMin()
                );

                // Best move stability time management
                // Less/more soft time the more/less stable the best root move is
                scale *= std::max<double>(
                    tmBestMovStabBase() - static_cast<double>(bestMoveStreak) * tmBestMovStabMul(),
                    tmBestMovStabMin()
                );

                const double originalSoftMs = static_cast<double>(*(mSearchConfig.softMs));
                return static_cast<u64>(originalSoftMs * scale);
            };

            if (msElapsed >= (td->rootDepth >= 6 ? softMsScaled() : mSearchConfig.softMs))
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

            // Fail low?
            if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max<i32>(alpha - delta, -INF);
                depth = td->rootDepth;
            }
            // Fail high?
            else if (score >= beta)
            {
                beta = std::min<i32>(beta + delta, INF);
                if (depth > 1) depth--;
            }
            else
                return score;

            delta = static_cast<i32>(static_cast<double>(delta) * aspDeltaMul());
        }
    }

    template<bool isRoot, bool pvNode, bool cutNode>
    constexpr i32 search(
        ThreadData* td,
        i32 depth,
        const size_t ply,
        i32 alpha,
        const i32 beta,
        const Move singularMove = MOVE_NONE)
    {
        assert(isRoot == (ply == 0));
        assert(ply >= 0 && ply <= MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);
        assert(!(isRoot && !pvNode));
        assert(pvNode || alpha + 1 == beta);
        assert(!(pvNode && cutNode));

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch<pvNode, true>(td, ply, alpha, beta);

        if (isHardTimeUp(td)) return 0;

        if constexpr (!isRoot)
        {
            const GameState gameState = td->pos.gameState(hasLegalMove, ply);

            if (gameState == GameState::Draw)
                return 0;

            if (gameState == GameState::Loss)
                return -INF + static_cast<i32>(ply);

            // Detect upcoming repetition (cuckoo)
            if (alpha < 0 && td->pos.hasUpcomingRepetition(ply) && (alpha = 0) >= beta)
                return 0;
        }

        depth = std::min<i32>(depth, MAX_DEPTH);

        // Probe TT for TT entry
        TTEntry& ttEntry = ttEntryRef(mTT, td->pos.zobristHash());

        // Get TT entry data
        const auto [ttDepth, ttScore, ttBound, ttMove]
            = ttEntry.get(td->pos.zobristHash(), static_cast<i16>(ply));

        // TT cutoff
        if (!pvNode
        && !singularMove
        && ttDepth >= depth
        && (ttBound == Bound::Exact
        || (ttBound == Bound::Upper && ttScore <= alpha)
        || (ttBound == Bound::Lower && ttScore >= beta)))
            return *ttScore;

        PlyData& plyData = td->pliesData[ply];
        const i32 eval = getEval(td, plyData);

        if (ply >= MAX_DEPTH) return eval;

        updateBothAccs(td);

        // Reset killer move of next tree level
        td->pliesData[ply + 1].killer = MOVE_NONE;

        // Node pruning
        if (!pvNode && !td->pos.inCheck() && !singularMove)
        {
            const i32 parentEval = *(td->pliesData[ply - 1].correctedEval);
            const bool oppWorsening = !td->pliesData[ply - 1].inCheck && eval > -parentEval;

            // RFP (Reverse futility pruning)
            if (depth <= 7
            && std::abs(beta) < MIN_MATE_SCORE
            && eval - beta >= std::max<i32>(depth - oppWorsening, 1) * rfpDepthMul())
                return (eval + beta) / 2;

            // Razoring
            if (alpha - eval > razoringBase() + depth * depth * razoringDepthMul())
                return qSearch<pvNode, true>(td, ply, alpha, beta);

            // NMP (Null move pruning)
            if (depth >= 3
            && td->pos.lastMove()
            && td->pos.stmHasNonPawns()
            && eval >= beta
            && !(ttBound == Bound::Upper && ttScore < beta))
            {
                makeMove(td, MOVE_NONE, ply + 1, &mTT);

                const i32 nmpDepth = depth - 4 - depth / 3;

                const i32 score = -search<false, false, false>(
                    td, nmpDepth, ply + 1, -beta, -alpha
                );

                undoMove(td);

                if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }
        }

        // IIR (Internal iterative reduction)
        if ((pvNode || cutNode) && depth >= 4 && !ttMove && !singularMove)
            depth--;

        size_t legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::Upper;

        // Keep track of fail low moves
        plyData.failLowNoisies.clear();
        plyData.failLowQuiets .clear();

        // Moves loop
        MovePicker mp = MovePicker(false, ttMove, plyData.killer, singularMove);
        while (true)
        {
            // Move, i32
            const auto [move, moveScore] = mp.nextLegal(td->pos, td->historyTable);

            if (!move) break;

            // In singular searches, singular move (TT move) isn't searched
            assert(move != singularMove);

            legalMovesSeen++;

            const bool isQuiet = td->pos.isQuiet(move);
            const float quietHist = static_cast<float>(isQuiet ? moveScore : 0);

            // Moves loop pruning at shallow depths
            if (!isRoot && bestScore > -MIN_MATE_SCORE && (isQuiet || moveScore < 0))
            {
                // LMP (Late move pruning)
                if (legalMovesSeen > static_cast<size_t>(3 + depth * depth))
                    break;

                const i32 lmrDepth
                    = depth - LMR_TABLE[static_cast<size_t>(depth)][isQuiet][legalMovesSeen];

                // FP (Futility pruning)

                const auto fpMargin = [lmrDepth, quietHist] () constexpr -> i32
                {
                    const i32 depthFactor     = std::max<i32>(lmrDepth, 1) * fpDepthMul();
                    const i32 quietHistFactor = static_cast<i32>(quietHist * fpQuietHistMul());

                    return std::max<i32>(fpBase() + depthFactor + quietHistFactor, 0);
                };

                if (!td->pos.inCheck()
                && lmrDepth <= 7
                && legalMovesSeen > 2
                && alpha < MIN_MATE_SCORE
                && alpha - eval > fpMargin())
                    break;

                // SEE pruning

                i32 threshold = isQuiet ? seeQuietThreshold() : seeNoisyThreshold();
                threshold *= depth;
                threshold -= static_cast<i32>(quietHist * seeQuietHistMul());
                threshold = std::min<i32>(threshold, -1);

                if (td->pos.stmHasNonPawns() && !td->pos.SEE(move, threshold))
                    continue;
            }

            i32 newDepth = depth - 1;

            // SE (Singular extensions)
            if (!isRoot
            && !singularMove
            && move == ttMove
            && static_cast<i32>(ply) < td->rootDepth * 2
            && depth >= 6
            && ttDepth >= depth - 3
            && std::abs(*ttScore) < MIN_MATE_SCORE
            && ttBound == Bound::Lower)
            {
                const i32 singularBeta = std::max<i32>(*ttScore - depth, -MIN_MATE_SCORE);

                const i32 singularScore = search<isRoot, false, cutNode>(
                    td, newDepth / 2, ply, singularBeta - 1, singularBeta, move
                );

                // Single or double extension
                if (singularScore < singularBeta)
                    newDepth += 1 + (singularBeta - singularScore > doubleExtMargin());
                // Multicut
                else if (singularScore >= beta && std::abs(singularScore) < MIN_MATE_SCORE)
                    return singularScore;
                // Negative extension
                else if (ttScore >= beta)
                    newDepth -= 3;
                // Expected cut-node negative extension
                else if constexpr (cutNode)
                    newDepth -= 2;

                plyData.failLowNoisies.clear();
                plyData.failLowQuiets .clear();
            }

            const u64 nodesBefore = td->nodes.load(std::memory_order_relaxed);

            makeMove(td, move, ply + 1, &mTT);

            const std::optional<PieceType> captured = td->pos.captured();

            // PVS (Principal variation search)

            i32 score = 0, numFailHighs = 0;

            // LMR (Late move reductions)
            if (depth >= 2 && legalMovesSeen > 1 + isRoot && (isQuiet || moveScore < 0))
            {
                // Base reduction
                i32 r = LMR_TABLE[static_cast<size_t>(depth)][isQuiet][legalMovesSeen];

                r -= pvNode;            // Reduce less if parent is an expected PV node
                r += cutNode * 2;       // Reduce more if parent node expects to fail high
                r -= td->pos.inCheck(); // Reduce moves that give check less

                // For quiet moves, less reduction the higher the move's history and vice-versa
                r -= lround(quietHist * lmrQuietHistMul());

                r = std::max<i32>(r, 0); // Don't extend

                const i32 reducedDepth = newDepth - r;

                // Reduced null window search
                score = -search<false, false, true>(
                    td, reducedDepth, ply + 1, -alpha - 1, -alpha
                );

                numFailHighs += score > alpha;

                if (score > alpha && reducedDepth < newDepth)
                {
                    // Deeper or shallower research?
                    newDepth += score - bestScore > deeperBase() + newDepth * 2;
                    newDepth -= score - bestScore < shallowerMargin();
                }

                if (score > alpha && reducedDepth < newDepth)
                {
                    // Null window research
                    score = -search<false, false, !cutNode>(
                        td, newDepth, ply + 1, -alpha - 1, -alpha
                    );

                    numFailHighs += score > alpha;
                }
            }
            else if (!pvNode || legalMovesSeen > 1)
            {
                // Null window search
                score = -search<false, false, !cutNode>(
                    td, newDepth, ply + 1, -alpha - 1, -alpha
                );

                numFailHighs += score > alpha;
            }

            // Full window search
            if (pvNode && (legalMovesSeen == 1 || score > alpha))
            {
                score = -search<false, true, false>(td, newDepth, ply + 1, -beta, -alpha);
                numFailHighs += score >= beta;
            }

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
                if (isQuiet)
                    plyData.failLowQuiets.push_back(move);
                else
                    plyData.failLowNoisies.push_back({ move, captured });

                continue;
            }

            alpha = score;
            bestMove = move;
            bound = Bound::Exact;

            // In PV nodes, update PV line
            if constexpr (pvNode)
            {
                plyData.pvLine.clear();
                plyData.pvLine.push_back(move);

                // Copy child's PV line
                for (const Move m : td->pliesData[ply + 1].pvLine)
                    plyData.pvLine.push_back(m);
            }

            // Fail high?
            if (score >= beta)
            {
                bound = Bound::Lower;

                if (isQuiet) plyData.killer = move;

                updateHistories(
                    td,
                    move,
                    captured,
                    depth,
                    numFailHighs,
                    plyData.failLowNoisies,
                    plyData.failLowQuiets
                );

                break;
            }
        }

        if (legalMovesSeen == 0)
        {
            assert(singularMove);
            return alpha;
        }

        if (!singularMove)
        {
            // Update TT entry
            ttEntry.update(
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
            && !(bound == Bound::Lower && bestScore <= eval)
            && !(bound == Bound::Upper && bestScore >= eval))
            {
                const i32 bonus = (bestScore - eval) * depth;

                for (i16* corrHistPtr : corrHistsPtrs(td))
                    updateHistory(corrHistPtr, bonus);
            }
        }

        assert(std::abs(bestScore) < INF);
        return bestScore;
    }

    // Quiescence search
    template<bool pvNode, bool isFirstCall>
    constexpr i32 qSearch(ThreadData* td, const size_t ply, i32 alpha, const i32 beta)
    {
        assert(ply > 0 && ply <= MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta) <= INF);
        assert(alpha < beta);
        assert(pvNode || alpha + 1 == beta);

        if (isHardTimeUp(td)) return 0;

        const GameState gameState = td->pos.gameState(hasLegalMove, ply);

        if (gameState == GameState::Draw)
            return 0;

        if (gameState == GameState::Loss)
            return -INF + static_cast<i32>(ply);

        // Detect upcoming repetition (cuckoo)
        if (alpha < 0 && td->pos.hasUpcomingRepetition(ply) && (alpha = 0) >= beta)
            return 0;

        if constexpr (!pvNode && isFirstCall)
        {
            // Probe TT for TT entry
            const TTEntry& ttEntry = ttEntryRef(mTT, td->pos.zobristHash());

            // Get TT entry data
            const auto [ttDepth, ttScore, ttBound, ttMove]
                = ttEntry.get(td->pos.zobristHash(), static_cast<i16>(ply));

            // TT cutoff
            if (ttBound == Bound::Exact
            || (ttBound == Bound::Upper && ttScore <= alpha)
            || (ttBound == Bound::Lower && ttScore >= beta))
                return *ttScore;
        }

        PlyData& plyData = td->pliesData[ply];
        const i32 eval = getEval(td, plyData);

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

        // Keep track of fail low moves
        plyData.failLowNoisies.clear();
        plyData.failLowQuiets .clear();

        // Moves loop (if not in check, only noisy moves)
        MovePicker mp = MovePicker(!td->pos.inCheck(), MOVE_NONE, plyData.killer);
        while (true)
        {
            // Move, i32
            const auto [move, moveScore] = mp.nextLegal(td->pos, td->historyTable);

            if (!move) break;

            // In check, we search all moves until we find a non-losing move
            // When that happens, we prune quiets and bad noisy moves
            // Also, if not in check, prune underpromotions
            if (bestScore > -MIN_MATE_SCORE && (td->pos.isQuiet(move) || moveScore < 0))
                break;

            // SEE pruning if not in check (skip bad noisy moves)
            if (!td->pos.inCheck() && !td->pos.SEE(move))
                continue;

            makeMove(td, move, ply + 1);

            const std::optional<PieceType> captured = td->pos.captured();

            const i32 score = -qSearch<pvNode, false>(td, ply + 1, -beta, -alpha);

            undoMove(td);

            if (mStopSearch.load(std::memory_order_relaxed))
                return 0;

            bestScore = std::max<i32>(bestScore, score);

            // Fail low?
            if (score <= alpha)
            {
                if (td->pos.isQuiet(move))
                    plyData.failLowQuiets.push_back(move);
                else
                    plyData.failLowNoisies.push_back({ move, captured });

                continue;
            }

            alpha = score;
            bestMove = move;

            // Fail high?
            if (score >= beta)
            {
                updateHistories(
                    td, move, captured, 1, 1, plyData.failLowNoisies, plyData.failLowQuiets
                );

                break;
            }
        }

        assert(std::abs(bestScore) < INF);
        return bestScore;
    }

}; // class Searcher
