// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256;

#include "board.hpp"
#include "search_params.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "nnue.hpp"
#include "ply_data.hpp"

inline u64 totalNodes();

MultiArray<i32, MAX_DEPTH+1, 256> LMR_TABLE = {}; // [depth][moveIndex]

constexpr void initLmrTable()
{
    LMR_TABLE = {};

    for (u64 depth = 1; depth < LMR_TABLE.size(); depth++)
        for (u64 move = 1; move < LMR_TABLE[0].size(); move++)
            LMR_TABLE[depth][move] = round(lmrBase() + ln(depth) * ln(move) * lmrMultiplier());
}

class SearchThread;

// in main.cpp, main thread is created and gMainThread = &gSearchThreads[0]
SearchThread* gMainThread = nullptr;

class SearchThread {
    private:

    Board mBoard;

    u8 mMaxDepth = MAX_DEPTH;
    u64 mNodes = 0;
    u8 mMaxPlyReached = 0; // seldepth

    std::chrono::time_point<std::chrono::steady_clock> mStartTime;

    u64 mSoftMilliseconds = I64_MAX, 
        mHardMilliseconds = I64_MAX,
        mSoftNodes = I64_MAX,
        mHardNodes = I64_MAX;

    std::array<PlyData, MAX_DEPTH+1> mPliesData = { };

    std::array<Accumulator, MAX_DEPTH+1> mAccumulators;
    Accumulator* mAccumulatorPtr = &mAccumulators[0];

    std::array<u64, 1ULL << 17> mMovesNodes; // [move]
    
    MultiArray<Move, 2, 1ULL << 17> mCountermoves = {}; // [nstm][lastMove]

    // [color][pieceType][targetSquare]
    MultiArray<HistoryEntry, 2, 6, 64> mHistoryTable = {};

    // [color][pawnHash % 16384]
    MultiArray<i32, 2, 16384> mCorrectionHistory = {};

    std::vector<TTEntry>* ttPtr = nullptr;
    
    public:

    inline static bool sSearchStopped = false; // This must be reset to false before calling search()

    inline SearchThread(std::vector<TTEntry>* ttPtr) {
        this->ttPtr = ttPtr;
    }

    inline void reset() {
        mCountermoves = {};
        mHistoryTable = {};
        mCorrectionHistory = {};
    }

    inline Move bestMoveRoot() {
        return mPliesData[0].mPvLine.size() == 0 ? MOVE_NONE : mPliesData[0].mPvLine[0];
    }

    inline u64 getNodes() { return mNodes; }

    inline u64 millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - mStartTime) / std::chrono::milliseconds(1);
    }

    inline i32 search(Board &board, i32 maxDepth, auto startTime, i64 softMilliseconds, 
                      i64 hardMilliseconds, i64 softNodes, i64 hardNodes)
    { 
        mBoard = board;
        mMaxDepth = std::clamp(maxDepth, 1, (i32)MAX_DEPTH);
        mStartTime = startTime;
        mSoftMilliseconds = std::max(softMilliseconds, (i64)0);
        mHardMilliseconds = std::max(hardMilliseconds, (i64)0);
        mSoftNodes = std::max(softNodes, (i64)0);
        mHardNodes = std::max(hardNodes, (i64)0);

        mPliesData[0] = PlyData();
        
        mAccumulators[0] = Accumulator(mBoard);
        mAccumulatorPtr = &mAccumulators[0];

        mNodes = 0;
        mMovesNodes = {};

        i32 score = 0;

        // ID (Iterative deepening)
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

            // Check soft limits
            if (nodes >= mSoftNodes
            || millisecondsElapsed() >= (iterationDepth >= nodesTmMinDepth()
                                         ? mSoftMilliseconds * nodesTmMultiplier()
                                           * (nodesTmBase() - bestMoveNodesFraction()) 
                                         : mSoftMilliseconds))
                break;
        }

        // Signal secondary threads to stop
        if (this == gMainThread) sSearchStopped = true;

        return score;
    }

    private:

    inline bool stopSearch()
    {
        // Only check time in main thread

        if (sSearchStopped) return true;

        if (this != gMainThread) return false;

        if (bestMoveRoot() == MOVE_NONE) return false;

        if (mHardNodes != I64_MAX && totalNodes() >= mHardNodes) 
            return sSearchStopped = true;

        // Check time every N nodes
        return sSearchStopped = (mNodes % 1024 == 0 && millisecondsElapsed() >= mHardMilliseconds);
    }

    inline double bestMoveNodesFraction() {
        return (double)mMovesNodes[bestMoveRoot().encoded()] / std::max((double)mNodes, 1.0);
    }

    inline i32 updateAccumulatorAndEval(i32 &eval) 
    {
        assert(mAccumulatorPtr == &mAccumulators[0] 
               ? mAccumulatorPtr->mUpdated 
               : (mAccumulatorPtr - 1)->mUpdated);

        if (!mAccumulatorPtr->mUpdated) 
            mAccumulatorPtr->update(mAccumulatorPtr - 1, mBoard);

        if (mBoard.inCheck())
            eval = INF;
        else if (eval == INF) {
            eval = evaluate(mAccumulatorPtr, mBoard, true);

            // Adjust eval with correction history
            int stm = (int)mBoard.sideToMove();
            eval += mCorrectionHistory[stm][mBoard.pawnHash() % 16384] / corrHistScale();

            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

    inline void makeMove(Move move, PlyData* plyDataPtr)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after the move
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) 
        {
            auto ttEntryIdx = TTEntryIndex(mBoard.roughHashAfter(move), ttPtr->size());
            __builtin_prefetch(&(*ttPtr)[ttEntryIdx]);
        }

        mBoard.makeMove(move);
        mNodes++;

        (plyDataPtr + 1)->mMovesGenerated = MoveGenType::NONE;
        (plyDataPtr + 1)->mPvLine.clear();
        (plyDataPtr + 1)->mEval = INF;

        if (move != MOVE_NONE) {
            mAccumulatorPtr++;
            mAccumulatorPtr->mUpdated = false;
        }
    }

    inline i32 aspiration(i32 iterationDepth, i32 score)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 delta = aspInitialDelta();
        i32 alpha = std::max(-INF, score - delta);
        i32 beta = std::min(INF, score + delta);
        i32 depth = iterationDepth;
        i32 bestScore = score;

        while (true) {
            score = search(depth, 0, alpha, beta, false, DOUBLE_EXTENSIONS_MAX);

            if (stopSearch()) return 0;

            if (score > bestScore) bestScore = score;

            if (score >= beta)
            {
                beta = std::min(beta + delta, INF);
                if (depth > 1) depth--;
            }
            else if (score <= alpha)
            {
                beta = (alpha + beta + bestScore) / 3;
                alpha = std::max(alpha - delta, -INF);
                depth = iterationDepth;
            }
            else
                break;

            delta *= aspDeltaMultiplier();
        }

        return score;
    }

    inline i32 search(i32 depth, i32 ply, i32 alpha, i32 beta, bool cutNode,
                      u8 doubleExtsLeft, bool singular = false)
    { 
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (stopSearch()) return 0;

        if (ply > mMaxPlyReached) mMaxPlyReached = ply; // update seldepth

        if (ply > 0 && alpha < 0 && mBoard.hasUpcomingRepetition(ply)) 
        {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        if (depth > mMaxDepth) depth = mMaxDepth;

        bool pvNode = beta > alpha + 1;
        assert(!(pvNode && cutNode));

        // Probe TT
        u64 ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = singular ? TTEntry() : (*ttPtr)[ttEntryIdx];
        bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash && ttEntry.getBound() != Bound::NONE;

        // TT cutoff (not done in singular searches since ttHit is false)
        if (ttHit 
        && !pvNode
        && ttEntry.depth >= depth 
        && (ttEntry.getBound() == Bound::EXACT
        || (ttEntry.getBound() == Bound::LOWER && ttEntry.score >= beta) 
        || (ttEntry.getBound() == Bound::UPPER && ttEntry.score <= alpha)))
            return ttEntry.adjustedScore(ply);

        PlyData* plyDataPtr = &mPliesData[ply];

        if (ply >= mMaxDepth)
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        Color stm = mBoard.sideToMove();
        (plyDataPtr + 1)->mKiller = MOVE_NONE;
        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        // If in check 2 plies ago, then (plyDataPtr - 2)->mEval is INF, and improving is false
        bool improving = ply > 1 && !mBoard.inCheck() && eval > (plyDataPtr - 2)->mEval;

        if (!pvNode && !mBoard.inCheck() && !singular)
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth() 
            && eval >= beta + (depth - improving) * rfpMultiplier())
                return (eval + beta) / 2;

            // Razoring
            if (depth <= razoringMaxDepth() 
            && eval + depth * razoringMultiplier() < alpha)
            {
                i32 score = qSearch(ply, alpha, beta);
                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && mBoard.lastMove() != MOVE_NONE 
            && eval >= beta
            && !(ttHit && ttEntry.getBound() == Bound::UPPER && ttEntry.score < beta)
            && mBoard.hasNonPawnMaterial(stm))
            {
                makeMove(MOVE_NONE, plyDataPtr);

                i32 score = 0;

                if (!mBoard.isDraw(ply)) {
                    i32 nmpDepth = depth - nmpBaseReduction() - depth / nmpReductionDivisor()
                                   - std::min((eval-beta) / nmpEvalBetaDivisor(), nmpEvalBetaMax());

                    score = -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtsLeft);
                }

                mBoard.undoMove();

                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }

            if (stopSearch()) return 0;
        }

        Move ttMove = ttHit ? Move(ttEntry.move) : MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() && ttMove == MOVE_NONE && !singular && (pvNode || cutNode))
            depth--;

        Move &countermove = mCountermoves[(int)mBoard.oppSide()][mBoard.lastMove().encoded()];

        if (!singular) 
            // genenerate and score all moves except underpromotions
            plyDataPtr->genAndScoreMoves(mBoard, false, ttMove, countermove, mHistoryTable);

        assert(!singular || plyDataPtr->mCurrentMoveIdx == 0);

        u64 pinned = mBoard.pinned();
        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        bool isBestMoveQuiet = false;
        Bound bound = Bound::UPPER;

        ArrayVec<Move, 256> quietFailLows;
        ArrayVec<i16*, 256> noisyFailLowsHistory;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove())
        {
            // skip illegal moves
            if (!mBoard.isPseudolegalLegal(move, pinned)) continue;

            legalMovesSeen++;

            PieceType captured = mBoard.captured(move);
            bool isQuiet = captured == PieceType::NONE && move.promotion() == PieceType::NONE;

            int pt = (int)move.pieceType();
            HistoryEntry &historyEntry = mHistoryTable[(int)stm][pt][move.to()];

            if (ply > 0 && bestScore > -MIN_MATE_SCORE && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + mBoard.inCheck()
                                      + depth * depth * lmpMultiplier() / (improving ? 1 : 2))
                    break;

                i32 lmrDepth = std::max(0, depth - LMR_TABLE[depth][legalMovesSeen] - !improving);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !mBoard.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + lmrDepth * fpMultiplier())
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth()) 
                {
                    i32 threshold = isQuiet ? lmrDepth * seeQuietThreshold() 
                                              - moveScore / seeQuietHistoryDiv()
                                            : depth * depth * seeNoisyThreshold() 
                                              - historyEntry.noisyHistory(captured) / seeNoisyHistoryDiv();

                    if (!SEE(mBoard, move, std::min(threshold, -1))) continue;
                }
            }

            i32 extension = 0;

            // SE (Singular extensions)
            // In singular searches, ttMove = MOVE_NONE, which prevents SE
            if (move == ttMove
            && ply > 0
            && depth >= singularMinDepth()
            && (i32)ttEntry.depth >= depth - singularDepthMargin()
            && abs(ttEntry.score) < MIN_MATE_SCORE
            && ttEntry.getBound() != Bound::UPPER) 
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                i32 singularBeta = std::max(-INF, (i32)ttEntry.score - i32(depth * singularBetaMultiplier()));

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           cutNode, doubleExtsLeft, true);

                // Double extension
                if (singularScore < singularBeta - doubleExtensionMargin() 
                && !pvNode 
                && doubleExtsLeft > 0)
                {
                    // singularScore is way lower than TT score
                    // TT move is probably MUCH better than all others, so extend its search by 2 plies
                    extension = 2;
                    doubleExtsLeft--;
                }
                // Normal singular extension
                else if (singularScore < singularBeta)
                    // TT move is probably better than all others, so extend its search by 1 ply
                    extension = 1;
                // Negative extensions
                else {
                    // some other move is probably better than TT move, so reduce TT move search
                    extension -= ttEntry.score >= beta; 
                    // reduce TT move search more if we expect to fail high
                    extension -= cutNode; 
                }

                plyDataPtr->mCurrentMoveIdx = 0; // reset since the singular search used this
            }

            u64 nodesBefore = mNodes;
            makeMove(move, plyDataPtr);

            // Check extension if no singular extensions
            if (extension == 0) extension = mBoard.inCheck();

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;

            if (mBoard.isDraw(ply)) goto moveSearched;

            if (legalMovesSeen == 1) {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, false, doubleExtsLeft);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && legalMovesSeen >= lmrMinMoves() && moveScore < COUNTERMOVE_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesSeen];
                lmr -= pvNode;           // reduce pv nodes less
                lmr -= mBoard.inCheck(); // reduce less if move gives check
                lmr -= improving;        // reduce less if were improving
                lmr += 2 * cutNode;      // reduce more if we expect to fail high
                
                // reduce moves with good history less and vice versa
                lmr -= round(isQuiet ? moveScore / (float)lmrQuietHistoryDiv()
                                     : historyEntry.noisyHistory(captured) / (float)lmrNoisyHistoryDiv());

                lmr = std::clamp(lmr, 0, depth - 2); // dont extend or reduce into qsearch
            }

            score = -search(depth - 1 - lmr + extension, ply + 1, -alpha-1, -alpha, true, doubleExtsLeft);

            if (score > alpha && lmr > 0)
                score = -search(depth - 1 + extension, ply + 1, -alpha-1, -alpha, !cutNode, doubleExtsLeft); 

            if (score > alpha && pvNode)
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, false, doubleExtsLeft);

            moveSearched:

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            if (ply == 0) mMovesNodes[move.encoded()] += mNodes - nodesBefore;

            if (score > bestScore) bestScore = score;

            // Fail low
            if (score <= alpha) {
                if (isQuiet) 
                    quietFailLows.push_back(move);
                else 
                    noisyFailLowsHistory.push_back(historyEntry.noisyHistoryPtr(captured));

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

            i32 historyBonus =  std::clamp(depth * historyBonusMultiplier() - historyBonusOffset(), 0, historyBonusMax());
            i32 historyMalus = -std::clamp(depth * historyMalusMultiplier() - historyMalusOffset(), 0, historyMalusMax());

            if (isQuiet) {
                // This fail high quiet is now a killer move
                plyDataPtr->mKiller = move; 

                // This fail high quiet is now a countermove
                if (mBoard.lastMove() != MOVE_NONE)
                    countermove = move;

                std::array<Move, 3> moves = {
                    mBoard.nthToLastMove(1), mBoard.nthToLastMove(2), mBoard.nthToLastMove(4)
                };

                // Increase history of this fail high quiet move
                historyEntry.updateQuietHistory(
                    historyBonus, 
                    plyDataPtr->mEnemyAttacks & bitboard(move.from()),
                    plyDataPtr->mEnemyAttacks & bitboard(move.to()),
                    moves);

                // History malus: decrease history of fail low quiets
                for (Move failLow : quietFailLows) {
                    pt = (int)failLow.pieceType();

                    mHistoryTable[(int)stm][pt][failLow.to()].updateQuietHistory(
                        historyMalus, 
                        plyDataPtr->mEnemyAttacks & bitboard(failLow.from()),
                        plyDataPtr->mEnemyAttacks & bitboard(failLow.to()),
                        moves);
                }
            }
            else 
                // Increase history of this fail high noisy move
                historyEntry.updateNoisyHistory(historyBonus, captured);

            // History malus: decrease history of fail low noisy moves
            for (i16* noisyHist : noisyFailLowsHistory)
                updateHistory(noisyHist, historyMalus);

            break; // Fail high / beta cutoff
        }

        if (legalMovesSeen == 0) 
            // checkmate or stalemate
            return mBoard.inCheck() ? -INF + ply : 0;

        if (!singular) {
            (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), depth, ply, bestScore, bestMove, bound);

            // Update correction history
            if (!mBoard.inCheck()
            && abs(bestScore) < MIN_MATE_SCORE
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                i32 &corrHist = mCorrectionHistory[(int)stm][mBoard.pawnHash() % 16384];
                i32 newWeight = std::min(depth + 1, corrHistNewWeightMax());
                
                corrHist *= corrHistScale() - newWeight;
                corrHist += (bestScore - eval) * corrHistScale() * newWeight;
                corrHist = std::clamp(corrHist / corrHistScale(), -corrHistMax(), corrHistMax());
            }
        }

        return bestScore;
    }

    // Quiescence search
    inline i32 qSearch(i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        if (ply > mMaxPlyReached) mMaxPlyReached = ply; // update seldepth

        if (alpha < 0 && mBoard.hasUpcomingRepetition(ply)) {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry* ttEntry = &(*ttPtr)[ttEntryIdx];
        bool ttHit = mBoard.zobristHash() == ttEntry->zobristHash && ttEntry->getBound() != Bound::NONE;

        // TT cutoff
        if (ttHit
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        PlyData* plyDataPtr = &mPliesData[ply];

        if (ply >= mMaxDepth)
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        if (!mBoard.inCheck()) {
            if (eval >= beta) return eval; 
            if (eval > alpha) alpha = eval;
        }

        (plyDataPtr + 1)->mKiller = MOVE_NONE;

        // if in check, generate all moves, else only noisy moves
        // never generate underpromotions
        Move ttMove = ttHit ? Move(ttEntry->move) : MOVE_NONE;
        Move countermove = mCountermoves[(int)mBoard.oppSide()][mBoard.lastMove().encoded()];
        plyDataPtr->genAndScoreMoves(mBoard, !mBoard.inCheck(), ttMove, countermove, mHistoryTable);

        u64 pinned = mBoard.pinned();
        int legalMovesPlayed = 0;
        i32 bestScore = mBoard.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove())
        {
            // SEE pruning (skip bad noisy moves)
            if (!mBoard.inCheck() && moveScore < 0) break;

            // skip illegal moves
            if (!mBoard.isPseudolegalLegal(move, pinned)) continue;

            makeMove(move, plyDataPtr);
            legalMovesPlayed++;

            i32 score = mBoard.isDraw(ply) ? 0 : -qSearch(ply + 1, -beta, -alpha);

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            if (score <= bestScore) continue;

            bestScore = score;
            bestMove = move;

            if (bestScore >= beta) {
                bound = Bound::LOWER;
                break;
            }
            
            if (bestScore > alpha) alpha = bestScore;
        }

        if (legalMovesPlayed == 0 && mBoard.inCheck()) 
            // checkmate
            return -INF + ply; 

        ttEntry->update(mBoard.zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
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