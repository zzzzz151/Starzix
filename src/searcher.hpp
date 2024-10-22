// clang-format off

#pragma once

#include "utils.hpp"
#include <deque>
#include <thread>

class Searcher {
    private:

    std::deque<ThreadData*> mThreadsData   = { };
    std::deque<std::thread> mNativeThreads = { };

    // Search limits
    std::chrono::time_point<std::chrono::steady_clock> mStartTime;
    u64 mMaxNodes, mHardMs, mSoftMs;

    bool mStopSearch = false;

    public:

    std::vector<TTEntry> mTT = { }; // Transposition table

    inline Searcher() {
        resizeTT(mTT, 32);
        printTTSize(mTT);

        addThread();
    }

    inline ThreadData* mainThreadData() { return mThreadsData[0]; }

    inline void reset() {
        for (ThreadData* td : mThreadsData)
        {
            td->board = START_BOARD;
            td->historyTable     = { };
            td->pawnsCorrHist    = { };
            td->nonPawnsCorrHist = { };
        }
    }

    inline Move bestMoveRoot() const {
        return mainThreadData()->pvLine.size() == 0
               ? mainThreadData()->pvLine[0] : MOVE_NONE;
    }

    inline u64 totalNodes() const
    {
        u64 nodes = 0;

        for (ThreadData* threadData : mThreadsData)
            nodes += threadData->nodes;

        return nodes;
    }

    inline void addThread() 
    {
        /*
        SearchThread* searchThread = new SearchThread();
        std::thread nativeThread([=]() mutable { thread->loop(); });

        mSearchThreads.push_back(searchThread);
        mNativeThreads.push_back(std::move(nativeThread));
        */
    }

    inline void removeThread()
    {

    }

    inline void loop()
    {
        
    }

    inline i32 search() {


        return search();
    }

    private:

    inline bool shouldStop(const ThreadData &td) 
    {
        if (mStopSearch) return true;

        // Only check stop conditions in main thread
        // Don't stop searching until a best root move is set
        if (&td != mainThreadData() || bestMoveRoot() == MOVE_NONE) 
            return false;

        if (mMaxNodes < std::numeric_limits<i64>::max() && totalNodes() >= mMaxNodes)
            return mStopSearch = true;

        // Check time every N nodes
        if (td.nodes % 1024 != 0) return false;

        return mStopSearch = (millisecondsElapsed(mStartTime) >= mHardMs);
    }

    inline i32 search(
        std::chrono::time_point<std::chrono::steady_clock> startTime, 
        const u64 hardMs, const u64 softMs, const i32 maxDepth, const u64 maxNodes)
    {
        mStartTime = startTime;
        mHardMs = hardMs;
        mSoftMs = softMs;
        mMaxNodes = maxNodes;



        return search();
    }   

    private:

    inline i32 search(ThreadData &td)
    { 
        td.nodes = 0;
        td.nodesByMove = { };

        td.pliesData[0] = PlyData();

        td.accumulators[0] = BothAccumulators(td.board);
        td.accumulatorPtr = &td.accumulators[0];

        // Reset finny table
        for (int color : {WHITE, BLACK})
            for (int mirrorHorizontally : {false, true})
                for (int inputBucket = 0; inputBucket < nnue::NUM_INPUT_BUCKETS; inputBucket++)
                {
                    FinnyTableEntry &finnyEntry = td->finnyTable[color][mirrorHorizontally][inputBucket];

                    if (mirrorHorizontally == mAccumulatorPtr->mMirrorHorizontally[color]
                    && inputBucket == mAccumulatorPtr->mInputBucket[color])
                    {
                        finnyEntry.accumulator = mAccumulatorPtr->mAccumulators[color];
                        td.board->getColorBitboards(finnyEntry.colorBitboards);
                        td.board->getPiecesBitboards(finnyEntry.piecesBitboards);
                    }
                    else {
                        finnyEntry.accumulator = nnue::NET->hiddenBiases[color];
                        finnyEntry.colorBitboards  = { };
                        finnyEntry.piecesBitboards = { };
                    }
                }

        // ID (Iterative deepening)
        i32 score = 0;
        for (i32 iterationDepth = 1; iterationDepth <= mMaxDepth; iterationDepth++)
        {
            mMaxPlyReached = 0;

            const i32 iterationScore = iterationDepth >= aspMinDepth() 
                                       ? aspiration(iterationDepth, score)
                                       : search(iterationDepth, 0, -INF, INF, false, DOUBLE_EXTENSIONS_MAX);

            if (shouldStop()) break;

            score = iterationScore;

            if (!printInfo) continue;

            // Print uci info

            std::cout << "info"
                      << " depth "    << iterationDepth
                      << " seldepth " << (int)mMaxPlyReached;

            if (abs(score) < MIN_MATE_SCORE)
                std::cout << " score cp " << score;
            else {
                const i32 movesTillMate = round((INF - abs(score)) / 2.0);
                std::cout << " score mate " << (score > 0 ? movesTillMate : -movesTillMate);
            }

            const u64 msElapsed = millisecondsElapsed();

            std::cout << " nodes " << mNodes
                      << " nps "   << mNodes * 1000 / std::max(msElapsed, (u64)1)
                      << " time "  << msElapsed
                      << " pv";

            for (const Move move : mPliesData[0].pvLine)
                std::cout << " " << move.toUci();

            std::cout << std::endl;

            // Check soft time limit (in case one exists)

            if (mSoftMilliseconds >= std::numeric_limits<i64>::max()) continue;

            // Nodes time management: scale soft time limit based on nodes spent on best move
            auto scaledSoftMs = [&]() -> u64 {
                const double bestMoveNodes = mMovesNodes[bestMoveRoot().encoded()];
                const double bestMoveNodesFraction = bestMoveNodes / std::max<double>(mNodes, 1.0);
                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);
                return (double)mSoftMilliseconds * (1.5 - bestMoveNodesFraction);
            };
                         
            if (msElapsed >= (iterationDepth >= aspMinDepth() ? scaledSoftMs() : mSoftMilliseconds))
                break;
        }

        return score;
    }

    inline i32 aspiration(ThreadData &td, const i32 iterationDepth, i32 score)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 depth = iterationDepth;
        i32 delta = aspInitialDelta();
        i32 alpha = std::max(-INF, score - delta);
        i32 beta  = std::min(INF,  score + delta);
        i32 bestScore = score;

        while (true) {
            score = search(td, depth, 0, alpha, beta, false, DOUBLE_EXTENSIONS_MAX);

            if (shouldStop()) return 0;

            if (score > bestScore) bestScore = score;

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
                break;

            delta *= aspDeltaMul();
        }

        return score;
    }

    inline i32 search(ThreadData &td, i32 depth, const i32 ply, i32 alpha, i32 beta, 
        const bool cutNode, u8 doubleExtsLeft, const Move singularMove = MOVE_NONE) 
    {
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (shouldStop(td)) return 0;

        // Cuckoo / detect upcoming repetition
        if (ply > 0 && alpha < 0 && td->board.hasUpcomingRepetition(ply)) 
        {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (depth > td.maxDepth) depth = td.maxDepth;

        const bool pvNode = beta > alpha + 1 || ply == 0;

        // Probe TT
        const auto ttEntryIdx = TTEntryIndex(td.board->zobristHash(), ttPtr->size());
        TTEntry ttEntry = singularMove != MOVE_NONE ? TTEntry() : (*ttPtr)[ttEntryIdx];
        const bool ttHit = td.board->zobristHash() == ttEntry.zobristHash;
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

        PlyData* plyDataPtr = &mPliesData[ply];

        // Max ply cutoff
        if (ply >= mMaxDepth) 
            return td.board->inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = updateAccumulatorAndEval(plyDataPtr->eval);

        const int improving = ply <= 1 || td.board->inCheck() || td.board->inCheck2PliesAgo()
                              ? 0
                              : eval - (plyDataPtr - 2)->eval >= improvingThreshold()
                              ? 1
                              : eval - (plyDataPtr - 2)->eval <= -improvingThreshold()
                              ? -1
                              : 0;

        (plyDataPtr + 1)->killer = MOVE_NONE;

        // Node pruning
        if (!pvNode && !td.board->inCheck() && singularMove == MOVE_NONE) 
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
                const i32 score = qSearch(ply, alpha, beta);

                if (shouldStop()) return 0;

                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && td.board->lastMove() != MOVE_NONE 
            && eval >= beta
            && eval >= beta + nmpEvalBetaMargin() - depth * nmpEvalBetaMul()
            && !(ttHit && ttEntry.bound() == Bound::UPPER && ttEntry.score < beta)
            && td.board->hasNonPawnMaterial(td.board->sideToMove()))
            {
                makeMove(MOVE_NONE, ply + 1);

                const i32 nmpDepth = depth - nmpBaseReduction() - depth * nmpDepthMul();
                
                const i32 score = td.board->isDraw(ply + 1) ? 0 
                                  : -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtsLeft);

                td.board->undoMove();

                if (shouldStop()) return 0;

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
                    depth, ply, probcutBeta, cutNode, doubleExtsLeft, ttMove, ttEntryIdx);

                if (shouldStop()) return 0;

                if (probcutScore != VALUE_NONE) return probcutScore;
            }
        }

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() 
        && (ttMove == MOVE_NONE || ttEntry.depth() < depth - iirMinDepth()) 
        && singularMove == MOVE_NONE 
        && (pvNode || cutNode))
            depth--;

        const bool lmrImproving = ply > 1 && !td.board->inCheck() && !td.board->inCheck2PliesAgo() 
                                  && eval > (plyDataPtr - 2)->eval;

        const int stm = int(td.board->sideToMove());
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

        while ((move = movePicker.next(mBoard, ttMove, plyDataPtr->killer, mMovesHistory, singularMove)) != MOVE_NONE)
        {
            assert(move != singularMove);

            legalMovesSeen++;
            const bool isQuiet = td.board->isQuiet(move);

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
            HistoryEntry &historyEntry = mMovesHistory[stm][pt][move.to()];

            i16* noisyHistoryPtr;
            if (!isQuiet) noisyHistoryPtr = historyEntry.noisyHistoryPtr(td.board->captured(move), move.promotion());

            // Moves loop pruning
            if (ply > 0 
            && bestScore > -MIN_MATE_SCORE 
            && legalMovesSeen >= 3 
            && (movePicker.stage() == MoveGenStage::QUIETS || movePicker.stage() == MoveGenStage::BAD_NOISIES))
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + td.board->inCheck()
                                      + depth * depth * lmpDepthMul())
                    break;
                    
                const i32 lmrDepth = depth - LMR_TABLE[isQuiet][depth][legalMovesSeen];

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !td.board->inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + std::max(lmrDepth + improving, 0) * fpDepthMul())
                    break;

                // SEE pruning

                const i32 threshold = isQuiet ? depth * seeQuietThreshold() - movePicker.moveScore() * seeQuietHistMul() 
                                              : depth * seeNoisyThreshold() - i32(*noisyHistoryPtr)  * seeNoisyHistMul();

                if (depth <= seePruningMaxDepth() && !td.board->SEE(move, threshold))
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
                    (depth - 1) / 2, ply, singularBeta - 1, singularBeta, cutNode, doubleExtsLeft, ttMove);

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

            const u64 nodesBefore = mNodes;
            makeMove(move, ply + 1);

            i32 score = 0;

            if (td.board->isDraw(ply + 1)) 
                goto moveSearched;

            // PVS (Principal variation search)

            // LMR (Late move reductions)
            if (depth >= 2 
            && !td.board->inCheck() 
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

                score = -search(newDepth - lmr, ply + 1, -alpha - 1, -alpha, true, doubleExtsLeft);

                if (score > alpha && lmr > 0) 
                {
                    // Deeper or shallower search?
                    newDepth += ply > 0 && score > bestScore + deeperBase() + newDepth * 2;
                    newDepth -= ply > 0 && score < bestScore + newDepth;

                    score = -search(newDepth, ply + 1, -alpha - 1, -alpha, !cutNode, doubleExtsLeft);
                }
            }
            else if (!pvNode || legalMovesSeen > 1)
                score = -search(newDepth, ply + 1, -alpha - 1, -alpha, !cutNode, doubleExtsLeft);

            if (pvNode && (legalMovesSeen == 1 || score > alpha))
                score = -search(newDepth, ply + 1, -beta, -alpha, false, doubleExtsLeft);

            moveSearched:

            td.board->undoMove();
            mAccumulatorPtr--;

            if (shouldStop()) return 0;

            assert(mNodes > nodesBefore);
            if (ply == 0) mMovesNodes[move.encoded()] += mNodes - nodesBefore;
            
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
            const Color nstm = td.board->oppSide();

            if (failLowQuiets.size() > 0)
                // Calling attacks(nstm) will cache enemy attacks and speedup isSquareAttacked()
                td.board->attacks(nstm);

            const std::array<Move, 3> lastMoves = { 
                td.board->lastMove(), td.board->nthToLastMove(2), td.board->nthToLastMove(4) 
            };

            // History bonus: increase this move's history
            historyEntry.updateQuietHistories(
                td.board->isSquareAttacked(move.from(), nstm), 
                td.board->isSquareAttacked(move.to(),   nstm),
                lastMoves, 
                bonus
            );

            // History malus: decrease history of fail low quiets
            for (const Move failLow : failLowQuiets) {
                pt = int(failLow.pieceType());

                mMovesHistory[stm][pt][failLow.to()].updateQuietHistories(
                    td.board->isSquareAttacked(failLow.from(), nstm),
                    td.board->isSquareAttacked(failLow.to(),   nstm),
                    lastMoves,
                    malus
                );
            }

            break;
        }

        if (legalMovesSeen == 0) {
            if (singularMove != MOVE_NONE) return alpha;

            assert(!td.board->hasLegalMove());

            // Checkmate or stalemate
            return td.board->inCheck() ? -INF + ply : 0;
        }

        assert(td.board->hasLegalMove());

        if (singularMove == MOVE_NONE) {
            // Store in TT
            (*ttPtr)[ttEntryIdx].update(td.board->zobristHash(), depth, ply, bestScore, bestMove, bound);

            // Update correction histories
            if (!td.board->inCheck()
            && abs(bestScore) < MIN_MATE_SCORE
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                for (i16* corrHist : correctionHistories())
                    if (corrHist != nullptr)
                        updateHistory(corrHist, (bestScore - eval) * depth);
            }

        }

        return bestScore;
    }

    inline i32 qSearch(const i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (shouldStop()) return 0;

        // Probe TT
        const auto ttEntryIdx = TTEntryIndex(td.board->zobristHash(), ttPtr->size());
        TTEntry ttEntry = (*ttPtr)[ttEntryIdx];
        const bool ttHit = td.board->zobristHash() == ttEntry.zobristHash;

        // TT cutoff
        if (ttHit) {
            ttEntry.adjustScore(ply);

            if (ttEntry.bound() == Bound::EXACT
            || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
            || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha))
                return ttEntry.score;
        }

        PlyData* plyDataPtr = &mPliesData[ply];

        // Max ply cutoff
        if (ply >= mMaxDepth)
            return td.board->inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = updateAccumulatorAndEval(plyDataPtr->eval);

        if (!td.board->inCheck()) {
            if (eval >= beta) return eval; 
            if (eval > alpha) alpha = eval;
        }
        
        i32 bestScore = td.board->inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop

        MovePicker movePicker = MovePicker(!td.board->inCheck());
        Move move;
        const Move ttMove = !ttHit || !td.board->inCheck() ? MOVE_NONE : Move(ttEntry.move);

        while ((move = movePicker.next(mBoard, ttMove, plyDataPtr->killer, mMovesHistory)) != MOVE_NONE)
        {
            assert([&]() {
                const MoveGenStage stage = movePicker.stage();
                const bool isNoisiesStage = stage == MoveGenStage::GOOD_NOISIES || stage == MoveGenStage::BAD_NOISIES;

                if (td.board->inCheck())
                    return stage == MoveGenStage::TT_MOVE_YIELDED
                           ? move == ttMove
                           : stage == MoveGenStage::KILLER_YIELDED 
                           ? move == plyDataPtr->killer && td.board->isQuiet(move)
                           : (td.board->isQuiet(move) ? stage == MoveGenStage::QUIETS : isNoisiesStage);

                return isNoisiesStage && td.board->isNoisyNotUnderpromo(move);
            }());

            // If in check, skip quiets and bad noisy moves
            if (td.board->inCheck() && bestScore > -MIN_MATE_SCORE && int(movePicker.stage()) > int(MoveGenStage::GOOD_NOISIES))
                break;

            // If not in check, skip bad noisy moves
            if (!td.board->inCheck() && !td.board->SEE(move))
                continue;
                
            makeMove(move, ply + 1);

            const i32 score = td.board->isDraw(ply + 1) ? 0 : -qSearch(ply + 1, -beta, -alpha);

            td.board->undoMove();
            mAccumulatorPtr--;

            if (shouldStop()) return 0;

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
            assert(!td.board->hasLegalMove());
            return -INF + ply;
        }

        // Store in TT
        (*ttPtr)[ttEntryIdx].update(td.board->zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    inline i32 probcut(const i32 depth, const i32 ply, const i32 probcutBeta, 
        const bool cutNode, const u8 doubleExtsLeft, const Move ttMove, const auto ttEntryIdx)
    {
        PlyData* plyDataPtr = &mPliesData[ply];

        // Moves loop

        MovePicker movePicker = MovePicker(true);
        Move move;

        while ((move = movePicker.next(mBoard, ttMove, plyDataPtr->killer, mMovesHistory)) != MOVE_NONE)
        {
            assert(movePicker.stage() == MoveGenStage::TT_MOVE_YIELDED
                ? move == ttMove
                : movePicker.stage() == MoveGenStage::GOOD_NOISIES || movePicker.stage() == MoveGenStage::BAD_NOISIES
            );

            assert(td.board->isNoisyNotUnderpromo(move));

            // SEE pruning (skip bad noisy moves)
            if (!td.board->SEE(move, probcutBeta - plyDataPtr->eval)) 
                continue;

            makeMove(move, ply + 1);

            i32 score = 0;

            if (td.board->isDraw(ply + 1))
                goto moveSearched;

            score = -qSearch(ply + 1, -probcutBeta, -probcutBeta + 1);

            if (score >= probcutBeta)
                score = -search(depth - 4, ply + 1, -probcutBeta, -probcutBeta + 1, !cutNode, doubleExtsLeft);

            moveSearched:

            td.board->undoMove();
            mAccumulatorPtr--;

            if (shouldStop()) return 0;

            if (score >= probcutBeta) {
                (*ttPtr)[ttEntryIdx].update(td.board->zobristHash(), depth - 3, ply, score, move, Bound::LOWER);
                return score;
            }

        }

        return VALUE_NONE;
    }

}; // class UciEngine

UciEngine uciEngine = UciEngine();
