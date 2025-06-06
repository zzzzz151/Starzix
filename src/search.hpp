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

enum class NodeType : i32 {
    PV, Cut, All
};

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
                  : search<true, NodeType::PV>(td, td->rootDepth, 0, -INF, INF);

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

                stableScoreStreak = std::abs(score - avgScore) <= tmScoreStabThreshold()
                                  ? stableScoreStreak + 1
                                  : 0;
            }

            // Keep track of best move stability for soft time scaling
            bestMoveStreak = bestMoveAtRoot(td) == prevBestMove ? bestMoveStreak + 1 : 0;

            const auto softMsScaled = [&] () constexpr -> u64
            {
                // Nodes TM
                // Less/more soft time the bigger/smaller the fraction of nodes spent on best move

                const u64 threadNodes   = td->nodes.load(std::memory_order_relaxed);
                const u64 bestMoveNodes = td->nodesByMove[bestMoveAtRoot(td).asU16()];

                const double bestMoveNodesFraction
                    = static_cast<double>(bestMoveNodes)
                    / static_cast<double>(std::max<u64>(threadNodes, 1));

                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);

                double scale = tmNodesBase() - bestMoveNodesFraction * tmNodesMul();

                // Score stability TM
                // Less/more soft time the more/less stable the root score is
                scale *= std::max<double>(
                    tmScoreStabBase() - static_cast<double>(stableScoreStreak) * tmScoreStabMul(),
                    tmScoreStabMin()
                );

                // Best move stability TM
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

        auto [alpha, beta] = score <= -MIN_MATE_SCORE
                           ? std::make_pair(-INF, -MIN_MATE_SCORE + 1)
                           : score >= MIN_MATE_SCORE
                           ? std::make_pair(MIN_MATE_SCORE - 1, INF)
                           : std::make_pair(score - delta, score + delta);

        while (true)
        {
            assert(alpha <  MIN_MATE_SCORE);
            assert(beta  > -MIN_MATE_SCORE);

            if (alpha <= -MIN_MATE_SCORE)
                alpha = -INF;

            if (beta >= MIN_MATE_SCORE)
                beta = INF;

            score = search<true, NodeType::PV>(td, depth, 0, alpha, beta);

            if (isHardTimeUp(td)) return 0;

            // Fail low?
            if (score <= alpha)
            {
                depth = td->rootDepth;

                alpha -= delta;

                if (std::abs(alpha) < MIN_MATE_SCORE && std::abs(beta) < MIN_MATE_SCORE)
                    beta = (alpha + beta) / 2;
            }
            // Fail high?
            else if (score >= beta)
            {
                depth -= depth > 1;
                beta  += delta;
            }
            else
                return score;

            delta = static_cast<i32>(static_cast<double>(delta) * aspDeltaMul());
        }
    }

    template<bool isRoot, NodeType nodeType>
    constexpr i32 search(
        ThreadData* td,
        i32 depth,
        const size_t ply,
        i32 alpha,
        const i32 beta,
        const Move singularMove = MOVE_NONE)
    {
        assert(!isRoot || nodeType == NodeType::PV);
        assert(isRoot == (ply == 0));
        assert(ply >= 0 && ply <= MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta)  <= INF);
        assert(alpha < beta);
        assert(nodeType == NodeType::PV || alpha + 1 == beta);

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch<nodeType == NodeType::PV>(td, ply, alpha, beta);

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
        if (nodeType != NodeType::PV
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
        if (nodeType != NodeType::PV && !td->pos.inCheck() && !singularMove)
        {
            const bool oppWorsening
                = !td->pliesData[ply - 1].inCheck && -eval < td->pliesData[ply - 1].correctedEval;

            // RFP (Reverse futility pruning)
            if (depth <= 7
            && std::abs(beta) < MIN_MATE_SCORE
            && eval - beta >= std::max<i32>(depth - oppWorsening, 1) * rfpDepthMul())
                return (eval + beta) / 2;

            // Razoring
            if (std::abs(alpha) < MIN_MATE_SCORE
            && alpha - eval > razoringBase() + depth * depth * razoringDepthMul())
                return qSearch<nodeType == NodeType::PV>(td, ply, alpha, beta);

            // NMP (Null move pruning)
            if (td->pos.lastMove()
            && td->pos.stmHasNonPawns()
            && depth >= 3
            && std::abs(beta) < MIN_MATE_SCORE
            && eval >= beta
            && (ttBound != Bound::Upper || ttScore >= beta))
            {
                makeMove(td, MOVE_NONE, ply + 1, mTT);

                const i32 nmpDepth = depth - 4 - depth / 3;

                const i32 score = -search<false, NodeType::All>(
                    td, nmpDepth, ply + 1, -beta, -alpha
                );

                undoMove(td);

                if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }

            // Probcut

            const i32 probcutBeta = std::min<i32>(beta + probcutMargin(), MIN_MATE_SCORE);

            if (depth >= 5
            && std::abs(beta) < MIN_MATE_SCORE
            && (ttBound == Bound::None || depth - *ttDepth >= 4 || ttScore >= probcutBeta))
            {
                const std::optional<i32> score = probcut<nodeType == NodeType::Cut>(
                    td, depth, ply, probcutBeta, ttMove, ttEntry
                );

                if (score.has_value()) return *score;
            }
        }

        // IIR (Internal iterative reduction)
        if (nodeType != NodeType::All && depth >= 4 && !ttMove && !singularMove)
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

            i32 reducedDepth
                = depth - 1 - LMR_TABLE[static_cast<size_t>(depth)][isQuiet][legalMovesSeen];

            // Reduce more if parent node expects to fail high
            reducedDepth -= (nodeType == NodeType::Cut) * 2;

            // Moves loop pruning at shallow depths
            if (!isRoot && bestScore > -MIN_MATE_SCORE && (isQuiet || moveScore < 0))
            {
                // LMP (Late move pruning)
                if (legalMovesSeen > static_cast<size_t>(2 + depth * depth))
                    break;

                // FP (Futility pruning)

                const auto fpMargin = [reducedDepth, quietHist] () constexpr -> i32
                {
                    const i32 margin = fpBase()
                                     + std::max<i32>(reducedDepth, 1) * fpDepthMul()
                                     + static_cast<i32>(quietHist * fpQuietHistMul());

                    return std::max<i32>(margin, 0);
                };

                if (!td->pos.inCheck()
                && depth <= 7
                && legalMovesSeen > 2
                && std::abs(alpha) < MIN_MATE_SCORE
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
            && depth - *ttDepth <= 3
            && std::abs(*ttScore) < MIN_MATE_SCORE
            && ttBound == Bound::Lower)
            {
                constexpr NodeType newNodeType
                    = nodeType == NodeType::Cut ? NodeType::Cut : NodeType::All;

                const i32 seBeta = std::max<i32>(*ttScore - depth, -MIN_MATE_SCORE);

                const i32 seScore = search<isRoot, newNodeType>(
                    td, newDepth / 2, ply, seBeta - 1, seBeta, move
                );

                // Single or double extension
                if (seScore < seBeta)
                {
                    newDepth++;
                    newDepth += seScore > -MIN_MATE_SCORE && seBeta - seScore > doubleExtMargin();
                }
                // Multicut
                else if (seScore >= beta && std::abs(seScore) < MIN_MATE_SCORE)
                    return seScore;
                // Negative extension
                else if (ttScore >= beta)
                    newDepth -= 3;
                // Expected cut-node negative extension
                else if constexpr (nodeType == NodeType::Cut)
                    newDepth -= 2;

                // The singular search used these so clear them
                plyData.failLowNoisies.clear();
                plyData.failLowQuiets .clear();
            }

            const u64 nodesBefore = td->nodes.load(std::memory_order_relaxed);

            makeMove(td, move, ply + 1, mTT);

            const std::optional<PieceType> captured = td->pos.captured();

            // PVS (Principal variation search)

            i32 score = 0, numFailHighs = 0;

            // Do full depth, zero window search?
            bool doFullDepthZws = nodeType != NodeType::PV || legalMovesSeen > 1;

            // LMR (Late move reductions)
            if (depth >= 2 && legalMovesSeen > 1 + isRoot && (isQuiet || moveScore < 0))
            {
                // Reduce less if parent is an expected PV node and TT score doesn't fail low
                reducedDepth += nodeType == NodeType::PV && ttScore.value_or(INF) > alpha;

                // Reduce moves that give check less
                reducedDepth += td->pos.inCheck();

                const bool improving
                    = ply >= 2
                    && !plyData.inCheck
                    && !td->pliesData[ply - 2].inCheck
                    && eval > td->pliesData[ply - 2].correctedEval;

                // Reduce less if parent's static eval is improving
                reducedDepth += improving;

                // For quiet moves, less reduction the higher the move's history and vice-versa
                reducedDepth += lround(quietHist * lmrQuietHistMul());

                // Don't reduce into quiescence search nor extend
                reducedDepth = std::clamp<i32>(reducedDepth, 1, newDepth);

                // Reduced depth, zero window search
                score = -search<false, NodeType::Cut>(
                    td, reducedDepth, ply + 1, -alpha - 1, -alpha
                );

                doFullDepthZws = score > alpha && reducedDepth < newDepth;

                const bool bothLoss = score <= -MIN_MATE_SCORE && bestScore <= -MIN_MATE_SCORE;
                const bool bothWin  = score >=  MIN_MATE_SCORE && bestScore >=  MIN_MATE_SCORE;

                // Deeper or shallower research?
                if (doFullDepthZws && !bothLoss && !bothWin)
                {
                    newDepth += score - bestScore > deeperBase() + newDepth * 2;
                    newDepth -= score - bestScore < shallowerMargin();

                    doFullDepthZws = reducedDepth < newDepth;
                }

                numFailHighs += score > alpha;
            }

            // Full depth, zero window search
            if (doFullDepthZws)
            {
                constexpr NodeType newNodeType
                    = nodeType == NodeType::Cut ? NodeType::All : NodeType::Cut;

                score = -search<false, newNodeType>(td, newDepth, ply + 1, -alpha - 1, -alpha);

                numFailHighs += score > alpha;
            }

            // Full depth, full window search
            if (nodeType == NodeType::PV && (legalMovesSeen == 1 || score > alpha))
            {
                score = -search<false, NodeType::PV>(td, newDepth, ply + 1, -beta, -alpha);
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
            if constexpr (nodeType == NodeType::PV)
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

        assert(std::abs(bestScore) < INF);

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
            && (bound != Bound::Lower || bestScore > eval)
            && (bound != Bound::Upper || bestScore < eval))
            {
                const i32 bonus = (bestScore - eval) * depth;

                for (i16* corrHistPtr : corrHistsPtrs(td))
                    updateHistory(corrHistPtr, bonus);
            }
        }

        return bestScore;
    }

    // Quiescence search
    template<bool pvNode>
    constexpr i32 qSearch(ThreadData* td, const size_t ply, i32 alpha, const i32 beta)
    {
        assert(ply > 0 && ply <= MAX_DEPTH);
        assert(std::abs(alpha) <= INF);
        assert(std::abs(beta)  <= INF);
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

        // Probe TT for TT entry
        TTEntry& ttEntry = ttEntryRef(mTT, td->pos.zobristHash());

        // Get TT entry data
        const auto [ttDepth, ttScore, ttBound, ttMove]
            = ttEntry.get(td->pos.zobristHash(), static_cast<i16>(ply));

        // TT cutoff
        if (!pvNode
        && (ttBound == Bound::Exact
        || (ttBound == Bound::Upper && ttScore <= alpha)
        || (ttBound == Bound::Lower && ttScore >= beta)))
            return *ttScore;

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

        const i32 fpValue = std::min<i32>(eval + fpQsMargin(), MIN_MATE_SCORE - 1);

        i32 bestScore = td->pos.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::Upper;

        // Keep track of fail low moves
        plyData.failLowNoisies.clear();
        plyData.failLowQuiets .clear();

        // Moves loop (if not in check, only noisy moves)
        MovePicker mp = MovePicker(!td->pos.inCheck(), ttMove, plyData.killer);
        while (true)
        {
            // Move, i32
            const auto [move, moveScore] = mp.nextLegal(td->pos, td->historyTable);

            if (!move) break;

            // In check, we search all moves
            // But once we find a non-losing move, we prune quiets and bad noisy moves
            // Also, if not in check, prune underpromotions (negative moveScore)
            if (bestScore > -MIN_MATE_SCORE && (td->pos.isQuiet(move) || moveScore < 0))
                break;

            // FP (Futility pruning)
            if (!td->pos.inCheck()
            && std::abs(alpha) < MIN_MATE_SCORE
            && fpValue <= alpha
            && !td->pos.SEE(move, 1))
            {
                bestScore = std::max<i32>(bestScore, fpValue);
                continue;
            }

            // SEE pruning (skip bad noisy moves)
            // Remember that if not in check, we only search noisy moves,
            // and in this case the move picker doesn't SEE
            if (!td->pos.inCheck() && !td->pos.SEE(move))
                continue;

            makeMove(td, move, ply + 1, mTT);

            const std::optional<PieceType> captured = td->pos.captured();

            const i32 score = -qSearch<pvNode>(td, ply + 1, -beta, -alpha);

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
                bound = Bound::Lower;

                updateHistories(
                    td, move, captured, 1, 1, plyData.failLowNoisies, plyData.failLowQuiets
                );

                break;
            }
        }

        assert(std::abs(bestScore) < INF);

        // Update TT entry
        ttEntry.update(
            td->pos.zobristHash(),
            0,
            static_cast<i16>(bestScore),
            static_cast<i16>(ply),
            bound,
            bestMove
        );

        return bestScore;
    }

    template<bool cutNode>
    constexpr std::optional<i32> probcut(
        ThreadData* td,
        const i32 depth,
        const size_t ply,
        const i32 probcutBeta,
        const Move ttMove,
        TTEntry& ttEntry)
    {
        assert(!td->pos.inCheck());
        assert(depth >= 5 && static_cast<size_t>(depth) <= MAX_DEPTH);
        assert(ply > 0 && ply <= MAX_DEPTH);
        assert(std::abs(probcutBeta) <= MIN_MATE_SCORE);

        const i32 eval = *(td->pliesData[ply].correctedEval);

        // Noisy moves loop
        MovePicker mp = MovePicker(true, ttMove, MOVE_NONE);
        while (true)
        {
            // Move, i32
            const auto [move, moveScore] = mp.nextLegal(td->pos, td->historyTable);

            // Prune underpromotions
            if (!move || move.isUnderpromotion())
                break;

            // SEE pruning (skip bad noisy moves)
            if (!td->pos.SEE(move, probcutBeta - eval))
                continue;

            makeMove(td, move, ply + 1, mTT);

            i32 score = -qSearch<false>(td, ply + 1, -probcutBeta, -probcutBeta + 1);

            if (score >= probcutBeta)
            {
                score = -search<false, cutNode ? NodeType::All : NodeType::Cut>(
                    td, depth - 4, ply + 1, -probcutBeta, -probcutBeta + 1
                );
            }

            undoMove(td);

            if (mStopSearch.load(std::memory_order_relaxed))
                return 0;

            if (score >= probcutBeta)
            {
                // Update TT entry
                ttEntry.update(
                    td->pos.zobristHash(),
                    static_cast<u8>(depth - 3),
                    static_cast<i16>(score),
                    static_cast<i16>(ply),
                    Bound::Lower,
                    move
                );

                return score;
            }
        }

        return std::nullopt;
    }

}; // class Searcher
