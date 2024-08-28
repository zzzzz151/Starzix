// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256,
              VALUE_NONE = INF + 1;

#include "board.hpp"
#include "search_params.hpp"
#include "history_entry.hpp"
#include "ply_data.hpp"
#include "tt.hpp"
#include "nnue.hpp"

inline u64 totalNodes();

MultiArray<i32, 2, MAX_DEPTH+1, 256> LMR_TABLE = {}; // [isQuietMove][depth][moveIndex]

constexpr void initLmrTable()
{
    LMR_TABLE = {};

    for (u64 depth = 1; depth < MAX_DEPTH+1; depth++)
        for (u64 move = 1; move < 256; move++)
        {
            LMR_TABLE[0][depth][move] 
                = round(lmrBaseNoisy() + ln(depth) * ln(move) * lmrMultiplierNoisy());

            LMR_TABLE[1][depth][move] 
                = round(lmrBaseQuiet() + ln(depth) * ln(move) * lmrMultiplierQuiet());
        }
}

class SearchThread;

// in main.cpp, main thread is created and gMainThread = &gSearchThreads[0]
SearchThread* gMainThread = nullptr;

class SearchThread {
    private:

    Board mBoard;

    std::chrono::time_point<std::chrono::steady_clock> mStartTime;

    u64 mHardMilliseconds = I64_MAX,
        mSoftMilliseconds = I64_MAX,
        mMaxNodes = I64_MAX;

    u8 mMaxDepth = MAX_DEPTH;
    u64 mNodes = 0;
    u8 mMaxPlyReached = 0; // seldepth

    std::array<PlyData, MAX_DEPTH+1> mPliesData; // [ply]

    std::array<BothAccumulators, MAX_DEPTH+1> mAccumulators;
    BothAccumulators* mAccumulatorPtr = &mAccumulators[0];

    MultiArray<HistoryEntry, 2, 6, 64> mMovesHistory = {}; // [stm][pieceType][targetSquare]

    MultiArray<Move, 2, 1ULL << 17> mCountermoves = {}; // [nstm][lastMove]

    std::array<u64, 1ULL << 17> mMovesNodes; // [move]

    // Correction histories
    MultiArray<i16, 2, 16384>    mPawnsCorrHist    = {}; // [stm][Board.pawnsHash() % 16384]
    MultiArray<i16, 2, 2, 16384> mNonPawnsCorrHist = {}; // [stm][pieceColor][Board.nonPawnsHash(pieceColor) % 16384]

    FinnyTable mFinnyTable; // [color][mirrorHorizontally][inputBucket]

    std::vector<TTEntry>* ttPtr = nullptr;

    u8 mTTAge = 0;
    
    public:

    inline static bool sSearchStopped = false; // This must be reset to false before calling search()

    inline SearchThread(std::vector<TTEntry>* ttPtr) {
        this->ttPtr = ttPtr;
    }

    inline void reset() {
        mMovesHistory = {};
        mCountermoves = {};
        mPawnsCorrHist = {};
        mNonPawnsCorrHist = {};
        mTTAge = 0;
    }

    inline u64 getNodes() { return mNodes; }

    inline u64 millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - mStartTime) / std::chrono::milliseconds(1);
    }

    inline Move bestMoveRoot() {
        return mPliesData[0].mPvLine.size() == 0 
               ? MOVE_NONE 
               : mPliesData[0].mPvLine[0];
    }

    inline i32 search(Board &board, i32 maxDepth, auto startTime, 
        i64 hardMilliseconds, i64 softMilliseconds, i64 maxNodes)
    { 
        mBoard = board;
        mStartTime = startTime;
        mHardMilliseconds = std::max(hardMilliseconds, (i64)0);
        mSoftMilliseconds = std::max(softMilliseconds, (i64)0);
        mMaxDepth = std::clamp(maxDepth, 1, (i32)MAX_DEPTH);
        mMaxNodes = std::max(maxNodes, (i64)0);

        mPliesData[0] = PlyData();

        mNodes = 0;
        mMovesNodes = {};

        mAccumulators[0] = BothAccumulators(mBoard);
        mAccumulatorPtr = &mAccumulators[0];

        // Reset finny table
        for (int color : {WHITE, BLACK})
            for (int mirrorHorizontally : {false, true})
                for (int inputBucket = 0; inputBucket < nnue::NUM_INPUT_BUCKETS; inputBucket++)
                {
                    FinnyTableEntry &finnyEntry = mFinnyTable[color][mirrorHorizontally][inputBucket];

                    if (mirrorHorizontally == mAccumulatorPtr->mMirrorHorizontally[color]
                    && inputBucket == mAccumulatorPtr->mInputBucket[color])
                    {
                        finnyEntry.accumulator = mAccumulatorPtr->mAccumulators[color];
                        mBoard.getColorBitboards(finnyEntry.colorBitboards);
                        mBoard.getPiecesBitboards(finnyEntry.piecesBitboards);
                    }
                    else {
                        finnyEntry.accumulator = nnue::NET->hiddenBiases[color];
                        finnyEntry.colorBitboards  = {};
                        finnyEntry.piecesBitboards = {};
                    }
                }

        // ID (Iterative deepening)
        i32 score = 0;
        for (i32 iterationDepth = 1; iterationDepth <= mMaxDepth; iterationDepth++)
        {
            mMaxPlyReached = 0;

            i32 iterationScore = iterationDepth >= aspMinDepth() 
                                 ? aspiration(iterationDepth, score)
                                 : search(iterationDepth, 0, -INF, INF, false, DOUBLE_EXTENSIONS_MAX);

            if (stopSearch()) break;

            score = iterationScore;

            if (this != gMainThread) continue;

            // Print uci info

            std::cout << "info"
                      << " depth "    << iterationDepth
                      << " seldepth " << (int)mMaxPlyReached;

            if (abs(score) < MIN_MATE_SCORE)
                std::cout << " score cp " << score;
            else {
                i32 movesTillMate = round((INF - abs(score)) / 2.0);
                std::cout << " score mate " << (score > 0 ? movesTillMate : -movesTillMate);
            }

            u64 nodes = totalNodes();
            u64 msElapsed = millisecondsElapsed();

            std::cout << " nodes " << nodes
                      << " nps "   << nodes * 1000 / std::max(msElapsed, (u64)1)
                      << " time "  << msElapsed
                      << " pv "    << mPliesData[0].pvString()
                      << std::endl;

            // Check soft time limit (in case one exists)

            if (mSoftMilliseconds == I64_MAX) continue;

            // Nodes time management: scale soft time limit based on nodes spent on best move
            auto scaledSoftMs = [&]() -> u64 {
                double bestMoveNodes = mMovesNodes[bestMoveRoot().encoded()];
                double bestMoveNodesFraction = bestMoveNodes / std::max<double>(mNodes, 1.0);
                assert(bestMoveNodesFraction >= 0.0 && bestMoveNodesFraction <= 1.0);
                
                return (double)mSoftMilliseconds * (1.5 - bestMoveNodesFraction);
            };
                         
            if (msElapsed >= (iterationDepth >= aspMinDepth() ? scaledSoftMs() : mSoftMilliseconds))
                break;
        }

        // Signal secondary threads to stop
        if (this == gMainThread) sSearchStopped = true;

        if (mTTAge < 127) mTTAge++;

        return score;
    }

    private:

    inline bool stopSearch()
    {
        // Only check time in main thread

        if (sSearchStopped) return true;

        if (this != gMainThread) return false;

        if (bestMoveRoot() == MOVE_NONE) return false;

        if (mMaxNodes != I64_MAX && totalNodes() >= mMaxNodes) 
            return sSearchStopped = true;

        // Check time every N nodes
        return sSearchStopped = (mNodes % 1024 == 0 && millisecondsElapsed() >= mHardMilliseconds);
    }

    inline void makeMove(Move move, i32 newPly)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after it
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) {
            auto ttEntryIdx = TTEntryIndex(mBoard.roughHashAfter(move), ttPtr->size());
            __builtin_prefetch(&(*ttPtr)[ttEntryIdx]);
        }

        mBoard.makeMove(move);
        mNodes++;

        // update seldepth
        if (newPly > mMaxPlyReached) mMaxPlyReached = newPly;

        mPliesData[newPly].softReset();
        
        if (move != MOVE_NONE) {
            mAccumulatorPtr++;
            mAccumulatorPtr->mUpdated = false;
        }
    }

    inline i32 updateAccumulatorAndEval(i32 &eval) 
    {
        assert(mAccumulatorPtr == &mAccumulators[0] 
               ? mAccumulatorPtr->mUpdated 
               : (mAccumulatorPtr - 1)->mUpdated);

        if (!mAccumulatorPtr->mUpdated) 
            mAccumulatorPtr->update(mAccumulatorPtr - 1, mBoard, mFinnyTable);

        if (mBoard.inCheck())
            eval = 0;
        else if (eval == VALUE_NONE) 
        {
            assert(BothAccumulators(mBoard) == *mAccumulatorPtr);

            eval = nnue::evaluate(mAccumulatorPtr, mBoard.sideToMove());

            eval *= materialScale(mBoard); // Scale eval with material

            // Adjust eval with correction histories

            i32 correction = 0;
            for (i16* corrHist : correctionHistories())
                correction += i32(*corrHist);

            eval += correction / corrHistScale();

            // /Clamp to avoid false mate scores and invalid scores
            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

    inline std::array<i16*, 3> correctionHistories()
    {
        int stm = (int)mBoard.sideToMove();

        return {
            &mPawnsCorrHist[stm][mBoard.pawnsHash() % 16384],
            &mNonPawnsCorrHist[stm][WHITE][mBoard.nonPawnsHash(Color::WHITE) % 16384],
            &mNonPawnsCorrHist[stm][BLACK][mBoard.nonPawnsHash(Color::BLACK) % 16384]
        };
    }

    inline i32 aspiration(i32 iterationDepth, i32 score)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 depth = iterationDepth;
        i32 delta = aspInitialDelta();
        i32 alpha = std::max(-INF, score - delta);
        i32 beta = std::min(INF, score + delta);
        i32 bestScore = score;

        while (true) {
            score = search(depth, 0, alpha, beta, false, DOUBLE_EXTENSIONS_MAX);

            if (stopSearch()) return 0;

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

            delta *= aspDeltaMultiplier();
        }

        return score;
    }

    inline i32 search(i32 depth, i32 ply, i32 alpha, i32 beta, 
        bool cutNode, u8 doubleExtsLeft, bool singular = false) 
    {
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        // Cuckoo / detect upcoming repetition
        if (ply > 0 && alpha < 0 && mBoard.hasUpcomingRepetition(ply)) 
        {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        // Quiescence search at leaf nodes
        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (depth > mMaxDepth) depth = mMaxDepth;

        const bool pvNode = beta > alpha + 1;

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = singular ? TTEntry() : (*ttPtr)[ttEntryIdx];
        const bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash;
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
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        int improving = ply <= 1 || mBoard.inCheck() || mBoard.inCheck2PliesAgo()
                        ? 0
                        : eval - (plyDataPtr - 2)->mEval >= improvingThreshold()
                        ? 1
                        : eval - (plyDataPtr - 2)->mEval <= -improvingThreshold()
                        ? -1
                        : 0;

        (plyDataPtr + 1)->mKiller = MOVE_NONE;

        // Node pruning
        if (!pvNode && !mBoard.inCheck() && !singular) 
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth() 
            && eval >= beta + (depth - improving) * rfpMultiplier())
                return eval;

            // Razoring
            if (depth <= razoringMaxDepth() 
            && abs(alpha) < 2000
            && eval + depth * razoringMultiplier() < alpha)
            {
                i32 score = qSearch(ply, alpha, beta);

                if (stopSearch()) return 0;

                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && mBoard.lastMove() != MOVE_NONE 
            && eval >= beta
            && eval >= beta + 90 - 13 * depth
            && !(ttHit && ttEntry.bound() == Bound::UPPER && ttEntry.score < beta)
            && mBoard.hasNonPawnMaterial(mBoard.sideToMove()))
            {
                makeMove(MOVE_NONE, ply + 1);

                i32 nmpDepth = depth - nmpBaseReduction() - depth / nmpReductionDivisor();
                
                i32 score = mBoard.isDraw(ply + 1) ? 0 
                            : -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtsLeft);

                mBoard.undoMove();

                if (stopSearch()) return 0;

				if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }

            // Probcut
            i32 probcutBeta = beta + probcutMargin() - improving * probcutMargin() * probcutImprovingPercentage();
            if (depth >= 5
            && abs(beta) < MIN_MATE_SCORE - 1
            && probcutBeta < MIN_MATE_SCORE - 1
            && !(ttHit && ttEntry.depth() >= depth - 3 && ttEntry.score < probcutBeta))
            {
                i32 probcutScore = probcut(
                    depth, ply, probcutBeta, cutNode, doubleExtsLeft, ttMove, ttEntryIdx);

                if (stopSearch()) return 0;

                if (probcutScore != VALUE_NONE) return probcutScore;
            }
        }

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() 
        && (ttMove == MOVE_NONE || ttEntry.depth() < depth - iirMinDepth()) 
        && !singular 
        && (pvNode || cutNode))
            depth--;

        const int stm = (int)mBoard.sideToMove();
        Move &countermove = mCountermoves[!stm][mBoard.lastMove().encoded()];

        // gen and score all moves, except underpromotions
        if (!singular) plyDataPtr->genAndScoreMoves(mBoard, ttMove, countermove, mMovesHistory);

        assert(!singular || plyDataPtr->mCurrentMoveIdx == 0);

        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        bool isBestMoveQuiet = false;
        Bound bound = Bound::UPPER;

        ArrayVec<Move, 256> failLowQuiets;
        ArrayVec<i16*, 256> failLowNoisiesHistory;

        bool lmrImproving = ply > 1 && !mBoard.inCheck() && !mBoard.inCheck2PliesAgo() 
                            && eval > (plyDataPtr - 2)->mEval;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(mBoard); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove(mBoard))
        {
            legalMovesSeen++;

            PieceType captured = mBoard.captured(move);
            bool isQuiet = captured == PieceType::NONE && move.promotion() == PieceType::NONE;

            // Moves loop pruning
            if (ply > 0 && bestScore > -MIN_MATE_SCORE && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + mBoard.inCheck()
                                      + depth * depth * lmpMultiplier())
                    break;
                    
                i32 lmrDepth = depth - LMR_TABLE[isQuiet][depth][legalMovesSeen];

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !mBoard.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + std::max(lmrDepth + improving, 0) * fpMultiplier())
                    break;

                // SEE pruning
                i32 threshold = depth * (isQuiet ? seeQuietThreshold() : seeNoisyThreshold());
                if (depth <= seePruningMaxDepth() && !mBoard.SEE(move, threshold))
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
            && (singularBeta = (i32)ttEntry.score - depth) > -MIN_MATE_SCORE + 1)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           cutNode, doubleExtsLeft, true);

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
                    
                plyDataPtr->mCurrentMoveIdx = 0; // reset since the singular search used this
            }

            u64 nodesBefore = mNodes;
            makeMove(move, ply + 1);

            int pt = (int)move.pieceType();
            HistoryEntry &historyEntry = mMovesHistory[stm][pt][move.to()];

            i32 score = 0;

            if (mBoard.isDraw(ply + 1)) 
                goto moveSearched;

            // PVS (Principal variation search)

            // LMR (Late move reductions)
            if (depth >= 2 && !mBoard.inCheck() && legalMovesSeen >= 2 && moveScore < COUNTERMOVE_SCORE)
            {
                i32 lmr = LMR_TABLE[isQuiet][depth][legalMovesSeen]
                          - pvNode       // reduce pv nodes less
                          - lmrImproving // reduce less if were improving
                          + 2 * cutNode; // reduce more if we expect to fail high
                
                // reduce moves with good history less and vice versa
                lmr -= round(
                    isQuiet ? moveScore / (float)lmrQuietHistoryDiv()
                            : historyEntry.noisyHistory(captured) / (float)lmrNoisyHistoryDiv()
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

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            assert(mNodes > nodesBefore);
            if (ply == 0) mMovesNodes[move.encoded()] += mNodes - nodesBefore;
            
            if (score > bestScore) bestScore = score;

            // Fail low?
            if (score <= alpha) {
                if (isQuiet) 
                    failLowQuiets.push_back(move);
                else
                    failLowNoisiesHistory.push_back(historyEntry.noisyHistoryPtr(captured));
                
                continue;
            }

            alpha = score;
            bestMove = move;
            isBestMoveQuiet = isQuiet;
            bound = Bound::EXACT;

            if (pvNode) plyDataPtr->updatePV(move);

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            i32 bonus =  std::clamp(depth * historyBonusMultiplier() - historyBonusOffset(), 0, historyBonusMax());
            i32 malus = -std::clamp(depth * historyMalusMultiplier() - historyMalusOffset(), 0, historyMalusMax());

            // History malus: decrease history of fail low noisy moves
            for (i16* noisyHistoryPtr : failLowNoisiesHistory)
                updateHistory(noisyHistoryPtr, malus);

            if (!isQuiet) {
                // History bonus: increase this move's history
                updateHistory(historyEntry.noisyHistoryPtr(captured), bonus);
                break;
            }

            // This move is a fail high quiet

            plyDataPtr->mKiller = move;
            
            if (mBoard.lastMove() != MOVE_NONE) countermove = move;

            const std::array<Move, 3> lastMoves = { 
                mBoard.lastMove(), mBoard.nthToLastMove(2), mBoard.nthToLastMove(4) 
            };
            
            // History bonus: increase this move's history
            historyEntry.updateQuietHistories(
                plyDataPtr->mEnemyAttacks & bitboard(move.from()), 
                plyDataPtr->mEnemyAttacks & bitboard(move.to()), 
                lastMoves,
                bonus);

            // History malus: decrease history of fail low quiets
            for (Move failLow : failLowQuiets) {
                pt = (int)failLow.pieceType();

                mMovesHistory[stm][pt][failLow.to()].updateQuietHistories(
                    plyDataPtr->mEnemyAttacks & bitboard(failLow.from()), 
                    plyDataPtr->mEnemyAttacks & bitboard(failLow.to()),  
                    lastMoves,
                    malus);
            }

            break;
        }

        if (legalMovesSeen == 0) {
            if (singular) return alpha;

            assert(!mBoard.hasLegalMove());

            // Checkmate or stalemate
            return mBoard.inCheck() ? -INF + ply : 0;
        }

        assert(mBoard.hasLegalMove());

        if (!singular) {
            // Store in TT
            (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), depth, ply, bestScore, bestMove, bound, mTTAge);

            // Update correction histories
            if (!mBoard.inCheck()
            && abs(bestScore) < MIN_MATE_SCORE
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                i32 newWeight = std::min(depth + depth * depth, corrHistScale());

                for (i16* corrHist : correctionHistories())
                {
                    i32 newValue = *corrHist;
                    newValue *= std::max(corrHistScale() - newWeight, 1);
                    newValue += (bestScore - eval) * corrHistScale() * newWeight;
                    *corrHist = std::clamp(newValue / corrHistScale(), -corrHistMax(), corrHistMax());
                }
            }

        }

        return bestScore;
    }

    inline i32 qSearch(i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = (*ttPtr)[ttEntryIdx];
        bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash;

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
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        if (!mBoard.inCheck()) {
            if (eval >= beta) return eval; 
            if (eval > alpha) alpha = eval;

            // not in check, generate and score noisy moves (except underpromotions)
            plyDataPtr->genAndScoreNoisyMoves(mBoard);
        }
        else {
            Move ttMove = ttHit ? Move(ttEntry.move) : MOVE_NONE;
            Move countermove = mCountermoves[(int)mBoard.oppSide()][mBoard.lastMove().encoded()];

            // in check, generate and score all moves (except underpromotions)
            plyDataPtr->genAndScoreMoves(mBoard, ttMove, countermove, mMovesHistory);
        }
        
        i32 bestScore = mBoard.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(mBoard); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove(mBoard))
        {
            // If in check, skip quiets and bad noisy moves
            if (mBoard.inCheck() && bestScore > -MIN_MATE_SCORE && moveScore <= KILLER_SCORE)
                break;

            // If not in check, skip bad noisy moves
            if (!mBoard.inCheck() && !mBoard.SEE(move))
                continue;
                
            makeMove(move, ply + 1);

            i32 score = mBoard.isDraw(ply + 1) ? 0 : -qSearch(ply + 1, -beta, -alpha);

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

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
            assert(!mBoard.hasLegalMove());
            return -INF + ply;
        }

        // Store in TT
        (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), 0, ply, bestScore, bestMove, bound, mTTAge);

        return bestScore;
    }

    inline i32 probcut(
        i32 depth, i32 ply, i32 probcutBeta, bool cutNode, u8 doubleExtsLeft, Move ttMove, auto ttEntryIdx)
    {
        PlyData* plyDataPtr = &mPliesData[ply];

        // generate and score noisy moves (except underpromotions)
        plyDataPtr->genAndScoreNoisyMoves(mBoard, ttMove);

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(mBoard); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove(mBoard))
        {
            // SEE pruning (skip bad noisy moves)
            if (!mBoard.SEE(move, probcutBeta - plyDataPtr->mEval)) 
                continue;

            makeMove(move, ply + 1);

            i32 score = 0;

            if (mBoard.isDraw(ply + 1))
                goto moveSearched;

            score = -qSearch(ply + 1, -probcutBeta, -probcutBeta + 1);

            if (score >= probcutBeta)
                score = -search(depth - 4, ply + 1, -probcutBeta, -probcutBeta + 1, !cutNode, doubleExtsLeft);

            moveSearched:

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            if (score >= probcutBeta) {
                (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), depth - 3, ply, score, move, Bound::LOWER, mTTAge);
                return score;
            }

        }

        return VALUE_NONE;
    }

}; // class SearchThread

// in main.cpp, main thread is created and gMainThread = &gSearchThreads[0]
std::vector<SearchThread> gSearchThreads;

inline u64 totalNodes() {
    u64 nodes = 0;

    for (SearchThread &searchThread : gSearchThreads)
        nodes += searchThread.getNodes();

    return nodes;
}
