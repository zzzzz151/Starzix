// clang-format off

#pragma once

#include "utils.hpp"
#include "search_params.hpp"
#include "thread_data.hpp"
#include "move_picker.hpp"
#include "history_entry.hpp"
#include "tt.hpp"
#include "nnue.hpp"
#include <thread>
#include <atomic>

class Searcher {
    private:

    std::vector<ThreadData*> mThreadsData   = { };
    std::vector<std::thread> mNativeThreads = { };

    // Search limits
    i32 mMaxDepth = MAX_DEPTH;
    u64 mMaxNodes = std::numeric_limits<u64>::max();
    std::chrono::time_point<std::chrono::steady_clock> mStartTime = std::chrono::steady_clock::now();
    u64 mHardMs = std::numeric_limits<u64>::max();
    u64 mSoftMs = std::numeric_limits<u64>::max();

    bool mPrintInfo = true;

    std::atomic<bool> mStopSearch = false;

    public:

    std::vector<TTEntry> mTT = { }; // Transposition table

    inline Searcher() {
        resizeTT(mTT, 32);
        setThreads(1);
    }

    ~Searcher() {
        setThreads(0);
    }

    inline void ucinewgame() 
    {
        board() = START_BOARD;
        mainThreadData()->pliesData[0].pvLine.clear(); // reset best root move

        resetTT(mTT);

        for (ThreadData* td : mThreadsData)
        {
            td->nodes = 0;
            td->historyTable     = { };
            td->pawnsCorrHist    = { };
            td->nonPawnsCorrHist = { };
        }
    }

    inline Board& board() const { return mainThreadData()->board; }

    inline Move bestMoveRoot() const 
    {
        return mainThreadData()->pliesData[0].pvLine.size() > 0
               ? mainThreadData()->pliesData[0].pvLine[0] 
               : MOVE_NONE;
    }

    inline u64 totalNodes() const
    {
        u64 nodes = 0;

        for (const ThreadData* td : mThreadsData)
            nodes += td->nodes;

        return nodes;
    }

    private:

    inline ThreadData* mainThreadData() const { return mThreadsData[0]; }

    inline void loop(ThreadData* td)
    {
        while (true) {
            std::unique_lock<std::mutex> lock(td->mutex);
            td->cv.wait(lock, [&] { return td->threadState != ThreadState::SLEEPING; });

            if (td->threadState == ThreadState::SEARCHING)
                iterativeDeepening(*td);
            else if (td->threadState == ThreadState::EXIT_ASAP)
                break;

            td->threadState = ThreadState::SLEEPING;
            td->cv.notify_one();
        }

        std::unique_lock<std::mutex> lock(td->mutex);
        td->threadState = ThreadState::EXITED;
        lock.unlock();
        td->cv.notify_all();
    }

    inline void blockUntilSleep() 
    {
        for (ThreadData* td : mThreadsData) 
        {
            std::unique_lock<std::mutex> lock(td->mutex);
            td->cv.wait(lock, [&] { return td->threadState == ThreadState::SLEEPING; });
        }
    }

    public:

    inline int setThreads(int numThreads) 
    {
        numThreads = std::clamp(numThreads, 0, 256);

        blockUntilSleep();

        // Remove threads
        while (!mThreadsData.empty())
        {
            ThreadData* lastThreadData = mThreadsData.back();

            lastThreadData->wake(ThreadState::EXIT_ASAP);

            {
                std::unique_lock<std::mutex> lock(lastThreadData->mutex);

                lastThreadData->cv.wait(lock, [lastThreadData] { 
                    return lastThreadData->threadState == ThreadState::EXITED; 
                });
            }

            if (mNativeThreads.back().joinable())
                mNativeThreads.back().join();

            mNativeThreads.pop_back();
            delete lastThreadData, mThreadsData.pop_back();
        }

        mThreadsData.reserve(numThreads);
        mNativeThreads.reserve(numThreads);

        // Add threads
        while (int(mThreadsData.size()) < numThreads)
        {
            ThreadData* threadData = new ThreadData();
            std::thread nativeThread([=, this]() mutable { loop(threadData); });

            mThreadsData.push_back(threadData);
            mNativeThreads.push_back(std::move(nativeThread));
        }

        mThreadsData.shrink_to_fit();
        mNativeThreads.shrink_to_fit();

        return numThreads;
    }

    inline std::pair<Move, i32> search(
        const i32 maxDepth, 
        const u64 maxNodes,
        const std::chrono::time_point<std::chrono::steady_clock> startTime, 
        const u64 hardMs, 
        const u64 softMs,
        const bool printInfo)
    {
        mMaxDepth = std::clamp(maxDepth, 1, MAX_DEPTH);
        mMaxNodes = maxNodes;
        mStartTime = startTime;
        mHardMs = hardMs;
        mSoftMs = softMs;

        mPrintInfo = printInfo;
        mStopSearch = false;

        blockUntilSleep();

        mainThreadData()->accumulators[0] = BothAccumulators(mainThreadData()->board);

        // Init main thread's finny table
        for (const int color : {WHITE, BLACK})
            for (const int mirrorHorizontally : {false, true})
                for (int inputBucket = 0; inputBucket < nnue::NUM_INPUT_BUCKETS; inputBucket++)
                {
                    FinnyTableEntry &finnyEntry = mainThreadData()->finnyTable[color][mirrorHorizontally][inputBucket];

                    if (mirrorHorizontally == mainThreadData()->accumulators[0].mMirrorHorizontally[color]
                    && inputBucket == mainThreadData()->accumulators[0].mInputBucket[color])
                    {
                        finnyEntry.accumulator = mainThreadData()->accumulators[0].mAccumulators[color];
                        mainThreadData()->board.getColorBitboards(finnyEntry.colorBitboards);
                        mainThreadData()->board.getPiecesBitboards(finnyEntry.piecesBitboards);
                    }
                    else {
                        finnyEntry.accumulator = nnue::NET->hiddenBiases[color];
                        finnyEntry.colorBitboards  = { };
                        finnyEntry.piecesBitboards = { };
                    }
                }

        // Init auxiliar threads
        for (size_t i = 1; i < mThreadsData.size(); i++)
        {
            mThreadsData[i]->board = mainThreadData()->board;
            mThreadsData[i]->accumulators[0] = mainThreadData()->accumulators[0];
            mThreadsData[i]->finnyTable = mainThreadData()->finnyTable;
        }

        for (ThreadData* td : mThreadsData)
            td->wake(ThreadState::SEARCHING);

        blockUntilSleep();

        return { bestMoveRoot(), mainThreadData()->score };
    }   

    private:

    inline void iterativeDeepening(ThreadData &td)
    { 
        td.nodes = 0;
        td.nodesByMove = { };

        td.pliesData[0] = PlyData();
        td.accumulatorPtr = &(td.accumulators[0]);

        td.score = VALUE_NONE;
        for (i32 iterationDepth = 1; iterationDepth <= mMaxDepth; iterationDepth++)
        {
            td.maxPlyReached = 0;

            const i32 iterationScore = iterationDepth >= aspMinDepth() 
                                       ? aspiration(td, iterationDepth)
                                       : search(td, iterationDepth, 0, -INF, INF, false, DOUBLE_EXTENSIONS_MAX);

            if (mStopSearch.load(std::memory_order_relaxed))
                break;

            td.score = iterationScore;

            // If not main thread, continue
            if (&td != mainThreadData()) continue;

            const u64 msElapsed = millisecondsElapsed(mStartTime);

            mStopSearch = msElapsed >= mHardMs
                          || (mMaxNodes < std::numeric_limits<i64>::max() && totalNodes() >= mMaxNodes);

            // Print uci info
            if (mPrintInfo)
            {
                std::cout << "info"
                          << " depth "    << iterationDepth
                          << " seldepth " << td.maxPlyReached;

                if (abs(td.score) < MIN_MATE_SCORE)
                    std::cout << " score cp " << td.score;
                else {
                    const i32 movesTillMate = round((INF - abs(td.score)) / 2.0);
                    std::cout << " score mate " << (td.score > 0 ? movesTillMate : -movesTillMate);
                }

                const u64 nodes = totalNodes();

                std::cout << " nodes " << nodes
                          << " nps "   << nodes * 1000 / std::max(msElapsed, (u64)1)
                          << " time "  << msElapsed
                          << " pv";

                for (const Move move : td.pliesData[0].pvLine)
                    std::cout << " " << move.toUci();

                std::cout << std::endl;
            }

            // Check soft time limit (in case one exists)

            if (mSoftMs >= std::numeric_limits<i64>::max()) continue;

            // Nodes time management: scale soft time limit based on nodes spent on best move
            auto scaledSoftMs = [&]() -> u64 {
                const double bestMoveNodes = td.nodesByMove[bestMoveRoot().encoded()];
                const double bestMoveNodesFraction = bestMoveNodes / std::max<double>(td.nodes, 1.0);
                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);
                return (double)mSoftMs * (1.5 - bestMoveNodesFraction);
            };
                         
            if (msElapsed >= (iterationDepth >= aspMinDepth() ? scaledSoftMs() : mSoftMs))
                break;
        }

        // If main thread, signal other threads to stop searching
        if (&td == mainThreadData()) mStopSearch = true;
    }

    inline bool shouldStop(const ThreadData &td) 
    {
        if (mStopSearch.load(std::memory_order_relaxed)) 
            return true;

        // Only check stop conditions and modify mStopSearch in main thread
        // Don't stop searching if depth 1 not completed
        if (&td != mainThreadData() || mainThreadData()->score == VALUE_NONE) 
            return false;

        if (mMaxNodes < std::numeric_limits<i64>::max() && totalNodes() >= mMaxNodes)
            return mStopSearch = true;

        // Check time every N nodes
        if (td.nodes % 1024 != 0) return false;

        return mStopSearch = (millisecondsElapsed(mStartTime) >= mHardMs);
    }

    inline i32 aspiration(ThreadData &td, const i32 iterationDepth)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 depth = iterationDepth;
        i32 delta = aspInitialDelta();
        i32 alpha = std::max(-INF, td.score - delta);
        i32 beta  = std::min(INF,  td.score + delta);

        while (true) {
            i32 score = search(td, depth, 0, alpha, beta, false, DOUBLE_EXTENSIONS_MAX);

            if (shouldStop(td)) return 0;

            if (score >= beta) {
                beta = std::min(beta + delta, INF);
                if (depth > 1) depth--;
            }
            else if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - delta, -INF);
                depth = iterationDepth;
            }
            else
                return score;

            delta *= aspDeltaMul();
        }
    }

    inline i32 search(ThreadData &td, i32 depth, const i32 ply, i32 alpha, i32 beta, 
        const bool cutNode, i32 doubleExtsLeft, const Move singularMove = MOVE_NONE) 
    {
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        // Cuckoo / detect upcoming repetition
        if (ply > 0 && alpha < 0 && td.board.hasUpcomingRepetition(ply)) 
        {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch(td, ply, alpha, beta);

        if (depth > mMaxDepth) depth = mMaxDepth;

        const bool pvNode = beta > alpha + 1 || ply == 0;

        // Probe TT
        const auto ttEntryIdx = TTEntryIndex(td.board.zobristHash(), mTT.size());
        TTEntry ttEntry = singularMove != MOVE_NONE ? TTEntry() : mTT[ttEntryIdx];
        const bool ttHit = td.board.zobristHash() == ttEntry.zobristHash;
        Move ttMove = MOVE_NONE;

        if (ttHit) {
            ttEntry.adjustScore(ply);
            ttMove = Move(ttEntry.move);

            // TT cutoff (not done in singular searches since ttHit is false)
            if (!pvNode
            && ttEntry.depth() >= depth 
            && (ttEntry.bound() == Bound::EXACT
            || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
            || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha)))
                return ttEntry.score;
        }

        PlyData* plyDataPtr = &(td.pliesData[ply]);

        // Max ply cutoff
        if (ply >= mMaxDepth) 
            return td.board.inCheck() ? 0 : td.updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = td.updateAccumulatorAndEval(plyDataPtr->eval);

        const int improving = ply <= 1 || td.board.inCheck() || td.board.inCheck2PliesAgo()
                              ? 0
                              : eval - (plyDataPtr - 2)->eval >= improvingThreshold()
                              ? 1
                              : eval - (plyDataPtr - 2)->eval <= -improvingThreshold()
                              ? -1
                              : 0;

        (plyDataPtr + 1)->killer = MOVE_NONE;

        // Node pruning
        if (!pvNode && !td.board.inCheck() && singularMove == MOVE_NONE) 
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth() 
            && eval >= beta + (depth - improving) * rfpDepthMul())
                return eval;

            // Razoring
            if (depth <= razoringMaxDepth() 
            && abs(alpha) < 2000
            && eval + depth * razoringDepthMul() < alpha)
            {
                const i32 score = qSearch(td, ply, alpha, beta);

                if (shouldStop(td)) return 0;

                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && td.board.lastMove() != MOVE_NONE 
            && eval >= beta
            && eval >= beta + nmpEvalBetaMargin() - depth * nmpEvalBetaMul()
            && !(ttHit && ttEntry.bound() == Bound::UPPER && ttEntry.score < beta)
            && td.board.hasNonPawnMaterial(td.board.sideToMove()))
            {
                td.makeMove(MOVE_NONE, ply + 1, mTT);

                const i32 nmpDepth = depth - nmpBaseReduction() - depth * nmpDepthMul();
                
                const i32 score = td.board.isDraw(ply + 1) ? 0 
                                  : -search(td, nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtsLeft);

                td.board.undoMove();

                if (shouldStop(td)) return 0;

                if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }

            // Probcut
            const i32 probcutBeta = beta + probcutMargin() - improving * probcutMargin() * probcutImprovingPercentage();
            if (depth >= 5
            && abs(beta) < MIN_MATE_SCORE - 1
            && probcutBeta < MIN_MATE_SCORE - 1
            && !(ttHit && ttEntry.depth() >= depth - 3 && ttEntry.score < probcutBeta))
            {
                const i32 probcutScore = probcut(
                    td, depth, ply, probcutBeta, cutNode, doubleExtsLeft, ttMove, ttEntryIdx);

                if (shouldStop(td)) return 0;

                if (probcutScore != VALUE_NONE) return probcutScore;
            }
        }

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() 
        && (ttMove == MOVE_NONE || ttEntry.depth() < depth - iirMinDepth()) 
        && singularMove == MOVE_NONE 
        && (pvNode || cutNode))
            depth--;

        const bool lmrImproving = ply > 1 && !td.board.inCheck() && !td.board.inCheck2PliesAgo() 
                                  && eval > (plyDataPtr - 2)->eval;

        const int stm = int(td.board.sideToMove());
        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        bool isBestMoveQuiet = false;
        Bound bound = Bound::UPPER;

        ArrayVec<Move, 256> failLowQuiets;
        ArrayVec<i16*, 256> failLowNoisiesHistory;

        // Moves loop

        MovePicker movePicker = MovePicker(false);
        Move move;

        while ((move = movePicker.next(
            td.board, ttMove, plyDataPtr->killer, td.historyTable, singularMove
            )) != MOVE_NONE)
        {
            assert(move != singularMove);

            legalMovesSeen++;
            const bool isQuiet = td.board.isQuiet(move);

            assert([&]() {
                const MoveGenStage stage = movePicker.stage();

                return stage == MoveGenStage::TT_MOVE_YIELDED 
                       ? move == ttMove && legalMovesSeen == 1
                       : stage == MoveGenStage::KILLER_YIELDED 
                       ? move == plyDataPtr->killer && isQuiet
                       : (isQuiet ? stage == MoveGenStage::QUIETS
                                  : stage == MoveGenStage::GOOD_NOISIES || stage == MoveGenStage::BAD_NOISIES
                         );
            }());

            int pt = int(move.pieceType());
            HistoryEntry &historyEntry = td.historyTable[stm][pt][move.to()];

            i16* noisyHistoryPtr;
            if (!isQuiet) noisyHistoryPtr = historyEntry.noisyHistoryPtr(td.board.captured(move), move.promotion());

            // Moves loop pruning
            if (ply > 0 
            && bestScore > -MIN_MATE_SCORE 
            && legalMovesSeen >= 3 
            && (movePicker.stage() == MoveGenStage::QUIETS || movePicker.stage() == MoveGenStage::BAD_NOISIES))
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + td.board.inCheck()
                                      + depth * depth * lmpDepthMul())
                    break;
                    
                const i32 lmrDepth = depth - LMR_TABLE[isQuiet][depth][legalMovesSeen];

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !td.board.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + std::max(lmrDepth + improving, 0) * fpDepthMul())
                    break;

                // SEE pruning

                const i32 threshold = isQuiet ? depth * seeQuietThreshold() - movePicker.moveScore() * seeQuietHistMul() 
                                              : depth * seeNoisyThreshold() - i32(*noisyHistoryPtr)  * seeNoisyHistMul();

                if (depth <= seePruningMaxDepth() && !td.board.SEE(move, threshold))
                    continue;
            }

            i32 newDepth = depth - 1;

            // SE (Singular extensions)
            // In singular searches, ttMove = MOVE_NONE, which prevents SE
            i32 singularBeta;
            if (move == ttMove
            && ply > 0
            && depth >= singularMinDepth()
            && ttEntry.depth() >= depth - singularDepthMargin()
            && ttEntry.bound() != Bound::UPPER 
            && abs(ttEntry.score) < MIN_MATE_SCORE
            && (singularBeta = i32(ttEntry.score) - depth) > -MIN_MATE_SCORE + 1)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                const i32 singularScore = search(
                    td, (depth - 1) / 2, ply, singularBeta - 1, singularBeta, cutNode, doubleExtsLeft, ttMove);

                // Double extension
                if (!pvNode && singularScore < singularBeta - doubleExtensionMargin() && doubleExtsLeft > 0) {
                    newDepth += 2;
                    doubleExtsLeft--;
                }
                // Normal singular extension
                else if (singularScore < singularBeta)
                    newDepth++;
                // Multicut
                else if (singularBeta >= beta) 
                    return singularBeta;
                // Negative extension
                else if (cutNode)
                    newDepth -= 2;
            }

            const u64 nodesBefore = td.nodes;
            td.makeMove(move, ply + 1, mTT);

            i32 score = 0;

            if (td.board.isDraw(ply + 1)) 
                goto moveSearched;

            // PVS (Principal variation search)

            // LMR (Late move reductions)
            if (depth >= 2 
            && !td.board.inCheck() 
            && legalMovesSeen >= 2 
            && (movePicker.stage() == MoveGenStage::QUIETS || movePicker.stage() == MoveGenStage::BAD_NOISIES))
            {
                i32 lmr = LMR_TABLE[isQuiet][depth][legalMovesSeen]
                          - pvNode       // reduce pv nodes less
                          - lmrImproving // reduce less if were improving
                          + 2 * cutNode; // reduce more if we expect to fail high

                // reduce moves with good history less and vice versa
                lmr -= round(
                    isQuiet ? float(movePicker.moveScore()) / (float)lmrQuietHistoryDiv()
                            : float(*noisyHistoryPtr) / (float)lmrNoisyHistoryDiv()
                );

                if (lmr < 0) lmr = 0; // dont extend

                score = -search(td, newDepth - lmr, ply + 1, -alpha - 1, -alpha, true, doubleExtsLeft);

                if (score > alpha && lmr > 0) 
                {
                    // Deeper or shallower search?
                    newDepth += ply > 0 && score > bestScore + deeperBase() + newDepth * 2;
                    newDepth -= ply > 0 && score < bestScore + newDepth;

                    score = -search(td, newDepth, ply + 1, -alpha - 1, -alpha, !cutNode, doubleExtsLeft);
                }
            }
            else if (!pvNode || legalMovesSeen > 1)
                score = -search(td, newDepth, ply + 1, -alpha - 1, -alpha, !cutNode, doubleExtsLeft);

            if (pvNode && (legalMovesSeen == 1 || score > alpha))
                score = -search(td, newDepth, ply + 1, -beta, -alpha, false, doubleExtsLeft);

            moveSearched:

            td.board.undoMove();
            td.accumulatorPtr--;

            if (shouldStop(td)) return 0;

            assert(td.nodes > nodesBefore);
            if (ply == 0) td.nodesByMove[move.encoded()] += td.nodes - nodesBefore;
            
            if (score > bestScore) bestScore = score;

            // Fail low?
            if (score <= alpha) {
                if (isQuiet) 
                    failLowQuiets.push_back(move);
                else
                    failLowNoisiesHistory.push_back(noisyHistoryPtr);

                continue;
            }

            alpha = score;
            bestMove = move;
            isBestMoveQuiet = isQuiet;
            bound = Bound::EXACT;

            // If PV node, update PV line
            if (pvNode) { 
                plyDataPtr->pvLine.clear();
                plyDataPtr->pvLine.push_back(move);

                // Copy child's PV line
                for (const Move move : (plyDataPtr + 1)->pvLine)
                    plyDataPtr->pvLine.push_back(move);
            }

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            const i32 bonus =  std::clamp(depth * historyBonusMul() - historyBonusOffset(), 0, historyBonusMax());
            const i32 malus = -std::clamp(depth * historyMalusMul() - historyMalusOffset(), 0, historyMalusMax());

            // History malus: decrease history of fail low noisy moves
            for (i16* noisyHistoryPtr : failLowNoisiesHistory)
                updateHistory(noisyHistoryPtr, malus);

            if (!isQuiet) {
                // History bonus: increase this move's history
                updateHistory(noisyHistoryPtr, bonus);
                break;
            }

            // This move is a fail high quiet

            plyDataPtr->killer = move;
            const Color nstm = td.board.oppSide();

            if (failLowQuiets.size() > 0)
                // Calling attacks(nstm) will cache enemy attacks and speedup isSquareAttacked()
                td.board.attacks(nstm);

            const std::array<Move, 3> lastMoves = { 
                td.board.lastMove(), td.board.nthToLastMove(2), td.board.nthToLastMove(4) 
            };

            // History bonus: increase this move's history
            historyEntry.updateQuietHistories(
                td.board.isSquareAttacked(move.from(), nstm), 
                td.board.isSquareAttacked(move.to(),   nstm),
                lastMoves, 
                bonus
            );

            // History malus: decrease history of fail low quiets
            for (const Move failLow : failLowQuiets) {
                pt = int(failLow.pieceType());

                td.historyTable[stm][pt][failLow.to()].updateQuietHistories(
                    td.board.isSquareAttacked(failLow.from(), nstm),
                    td.board.isSquareAttacked(failLow.to(),   nstm),
                    lastMoves,
                    malus
                );
            }

            break;
        }

        if (legalMovesSeen == 0) {
            if (singularMove != MOVE_NONE) return alpha;

            assert(!td.board.hasLegalMove());

            // Checkmate or stalemate
            return td.board.inCheck() ? -INF + ply : 0;
        }

        assert(td.board.hasLegalMove());

        if (singularMove == MOVE_NONE) {
            // Store in TT
            mTT[ttEntryIdx].update(td.board.zobristHash(), depth, ply, bestScore, bestMove, bound);

            // Update correction histories
            if (!td.board.inCheck()
            && abs(bestScore) < MIN_MATE_SCORE
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                for (i16* corrHist : td.correctionHistories())
                    if (corrHist != nullptr) updateHistory(corrHist, (bestScore - eval) * depth);
            }
        }

        return bestScore;
    }

    inline i32 qSearch(ThreadData &td, const i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        // Probe TT
        const auto ttEntryIdx = TTEntryIndex(td.board.zobristHash(), mTT.size());
        TTEntry ttEntry = mTT[ttEntryIdx];
        const bool ttHit = td.board.zobristHash() == ttEntry.zobristHash;

        // TT cutoff
        if (ttHit) {
            ttEntry.adjustScore(ply);

            if (ttEntry.bound() == Bound::EXACT
            || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
            || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha))
                return ttEntry.score;
        }

        PlyData* plyDataPtr = &(td.pliesData[ply]);

        // Max ply cutoff
        if (ply >= mMaxDepth)
            return td.board.inCheck() ? 0 : td.updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = td.updateAccumulatorAndEval(plyDataPtr->eval);

        if (!td.board.inCheck()) {
            if (eval >= beta) return eval; 
            if (eval > alpha) alpha = eval;
        }
        
        i32 bestScore = td.board.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop

        MovePicker movePicker = MovePicker(!td.board.inCheck());
        Move move;
        const Move ttMove = !ttHit || !td.board.inCheck() ? MOVE_NONE : Move(ttEntry.move);

        while ((move = movePicker.next(td.board, ttMove, plyDataPtr->killer, td.historyTable)) != MOVE_NONE)
        {
            assert([&]() {
                const MoveGenStage stage = movePicker.stage();
                const bool isNoisiesStage = stage == MoveGenStage::GOOD_NOISIES || stage == MoveGenStage::BAD_NOISIES;

                if (td.board.inCheck())
                    return stage == MoveGenStage::TT_MOVE_YIELDED
                           ? move == ttMove
                           : stage == MoveGenStage::KILLER_YIELDED 
                           ? move == plyDataPtr->killer && td.board.isQuiet(move)
                           : (td.board.isQuiet(move) ? stage == MoveGenStage::QUIETS : isNoisiesStage);

                return isNoisiesStage && td.board.isNoisyNotUnderpromo(move);
            }());

            // If in check, skip quiets and bad noisy moves
            if (td.board.inCheck() && bestScore > -MIN_MATE_SCORE && int(movePicker.stage()) > int(MoveGenStage::GOOD_NOISIES))
                break;

            // If not in check, skip bad noisy moves
            if (!td.board.inCheck() && !td.board.SEE(move))
                continue;
                
            td.makeMove(move, ply + 1, mTT);

            const i32 score = td.board.isDraw(ply + 1) ? 0 : -qSearch(td, ply + 1, -beta, -alpha);

            td.board.undoMove();
            td.accumulatorPtr--;

            if (shouldStop(td)) return 0;

            if (score <= bestScore) continue;

            bestScore = score;

            if (bestScore > alpha) {
                alpha = bestScore;
                bestMove = move;
            }

             // Fail high / beta cutoff
            if (bestScore >= beta) {
                bound = Bound::LOWER;
                break;
            }
        }

        // Checkmate?
        if (bestScore == -INF) {
            assert(!td.board.hasLegalMove());
            return -INF + ply;
        }

        // Store in TT
        mTT[ttEntryIdx].update(td.board.zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    inline i32 probcut(ThreadData &td, const i32 depth, const i32 ply, const i32 probcutBeta, 
        const bool cutNode, const u8 doubleExtsLeft, const Move ttMove, const auto ttEntryIdx)
    {
        PlyData* plyDataPtr = &(td.pliesData[ply]);

        // Moves loop

        MovePicker movePicker = MovePicker(true);
        Move move;

        while ((move = movePicker.next(td.board, ttMove, plyDataPtr->killer, td.historyTable)) != MOVE_NONE)
        {
            assert(movePicker.stage() == MoveGenStage::TT_MOVE_YIELDED
                ? move == ttMove
                : movePicker.stage() == MoveGenStage::GOOD_NOISIES || movePicker.stage() == MoveGenStage::BAD_NOISIES
            );

            assert(td.board.isNoisyNotUnderpromo(move));

            // SEE pruning (skip bad noisy moves)
            if (!td.board.SEE(move, probcutBeta - plyDataPtr->eval)) 
                continue;

            td.makeMove(move, ply + 1, mTT);

            i32 score = 0;

            if (td.board.isDraw(ply + 1))
                goto moveSearched;

            score = -qSearch(td, ply + 1, -probcutBeta, -probcutBeta + 1);

            if (score >= probcutBeta)
                score = -search(td, depth - 4, ply + 1, -probcutBeta, -probcutBeta + 1, !cutNode, doubleExtsLeft);

            moveSearched:

            td.board.undoMove();
            td.accumulatorPtr--;

            if (shouldStop(td)) return 0;

            if (score >= probcutBeta) {
                mTT[ttEntryIdx].update(td.board.zobristHash(), depth - 3, ply, score, move, Bound::LOWER);
                return score;
            }

        }

        return VALUE_NONE;
    }

}; // class Searcher
