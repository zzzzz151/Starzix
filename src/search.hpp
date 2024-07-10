// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256;

#include "board.hpp"
#include "search_params.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "ply_data.hpp"
#include "tt.hpp"
#include "nnue.hpp"

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

    std::chrono::time_point<std::chrono::steady_clock> mStartTime;

    u64 mHardMilliseconds = I64_MAX,
        mSoftMilliseconds = I64_MAX,
        mMaxNodes = I64_MAX;

    u8 mMaxDepth = MAX_DEPTH;
    u64 mNodes = 0;
    u8 mMaxPlyReached = 0; // seldepth

    std::array<PlyData, MAX_DEPTH+1> mPliesData; // [ply]

    std::array<Accumulator, MAX_DEPTH+1> mAccumulators;
    Accumulator* mAccumulatorPtr = &mAccumulators[0];

    MultiArray<HistoryEntry, 2, 6, 64> mMovesHistory = {}; // [stm][pieceType][targetSquare]

    std::array<u64, 1ULL << 17> mMovesNodes; // [move]

    std::vector<TTEntry>* ttPtr = nullptr;
    
    public:

    inline static bool sSearchStopped = false; // This must be reset to false before calling search()

    inline SearchThread(std::vector<TTEntry>* ttPtr) {
        this->ttPtr = ttPtr;
    }

    inline void reset() {
        mMovesHistory = {};
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

        mAccumulators[0] = Accumulator(mBoard);
        mAccumulatorPtr = &mAccumulators[0];

        mNodes = 0;
        mMovesNodes = {};

        // ID (Iterative deepening)
        i32 score = 0;
        for (i32 iterationDepth = 1; iterationDepth <= mMaxDepth; iterationDepth++)
        {
            mMaxPlyReached = 0;

            i32 iterationScore = iterationDepth >= aspMinDepth() 
                                 ? aspiration(iterationDepth, score)
                                 : search(iterationDepth, 0, -INF, INF, DOUBLE_EXTENSIONS_MAX);

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
                
                return mSoftMilliseconds * nodesTmMultiplier() * (nodesTmBase() - bestMoveNodesFraction);
            };
                         
            if (msElapsed >= (iterationDepth >= aspMinDepth() ? scaledSoftMs() : mSoftMilliseconds))
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

        if (mMaxNodes != I64_MAX && totalNodes() >= mMaxNodes) 
            return sSearchStopped = true;

        // Check time every N nodes
        return sSearchStopped = (mNodes % 1024 == 0 && millisecondsElapsed() >= mHardMilliseconds);
    }

    inline void makeMove(Move move, PlyData* plyDataPtr)
    {
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

    inline i32 updateAccumulatorAndEval(i32 &eval) 
    {
        assert(mAccumulatorPtr == &mAccumulators[0] 
               ? mAccumulatorPtr->mUpdated 
               : (mAccumulatorPtr - 1)->mUpdated);

        if (!mAccumulatorPtr->mUpdated) 
            mAccumulatorPtr->update(mAccumulatorPtr - 1, mBoard);

        if (mBoard.inCheck())
            eval = 0;
        else if (eval == INF) {
            eval = evaluate(mAccumulatorPtr, mBoard, true);
            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

    inline i32 aspiration(i32 iterationDepth, i32 score)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 delta = aspInitialDelta();
        i32 alpha = std::max(-INF, score - delta);
        i32 beta = std::min(INF, score + delta);
        i32 bestScore = score;

        while (true) {
            score = search(iterationDepth, 0, alpha, beta, DOUBLE_EXTENSIONS_MAX);

            if (stopSearch()) return 0;

            if (score > bestScore) bestScore = score;

            if (score >= beta)
                beta = std::min(beta + delta, INF);
            else if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - delta, -INF);
            }
            else
                break;

            delta *= aspDeltaMultiplier();
        }

        return score;
    }

    inline i32 search(i32 depth, i32 ply, i32 alpha, i32 beta, u8 doubleExtsLeft, bool singular = false) {
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        if (ply > mMaxPlyReached) mMaxPlyReached = ply; // update seldepth

        // Cuckoo / detect upcoming repetition
        if (ply > 0 && alpha < 0 && mBoard.hasUpcomingRepetition(ply)) 
        {
            alpha = 0;
            if (alpha >= beta) return alpha;
        }

        // Quiescence search at leaf nodes
        if (depth <= 0) { 
            assert(!mBoard.inCheck());
            return qSearch(ply, alpha, beta);
        }

        if (depth > mMaxDepth) depth = mMaxDepth;

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = singular ? TTEntry() : (*ttPtr)[ttEntryIdx];
        bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash && ttEntry.bound() != Bound::NONE;

        // TT cutoff (not done in singular searches since ttHit is false)
        if (ttHit 
        && ply > 0
        && ttEntry.depth >= depth 
        && (ttEntry.bound() == Bound::EXACT
        || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
        || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha)))
            return ttEntry.adjustedScore(ply);

        PlyData* plyDataPtr = &mPliesData[ply];

        // Max ply cutoff
        if (ply >= mMaxDepth) 
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        bool pvNode = beta > alpha + 1;
        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);
        (plyDataPtr + 1)->mKiller = MOVE_NONE;

        // Node pruning
        if (!pvNode && !mBoard.inCheck() && !singular) 
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth() 
            && eval >= beta + depth * rfpMultiplier())
                return eval;

            // Razoring
            if (depth <= razoringMaxDepth() 
            && abs(alpha) < 2000
            && eval + depth * razoringMultiplier() < alpha)
            {
                i32 score = qSearch(ply, alpha, beta);
                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && mBoard.lastMove() != MOVE_NONE 
            && eval >= beta
            && mBoard.hasNonPawnMaterial(mBoard.sideToMove()))
            {
                makeMove(MOVE_NONE, plyDataPtr);

                i32 score = 0;

                if (!mBoard.isDraw(ply)) {
                    i32 nmpDepth = depth - nmpBaseReduction() - depth / nmpReductionDivisor();
                    score = -search(nmpDepth, ply + 1, -beta, -alpha, doubleExtsLeft);
                }

                mBoard.undoMove();

				if (score >= beta) return score >= MIN_MATE_SCORE ? beta : score;
            }
        }

        Move ttMove = ttHit ? Move(ttEntry.move) : MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() && ttMove == MOVE_NONE && pvNode && !singular)
            depth--;

        if (!singular)
            // gen and score all moves, except underpromotions
            plyDataPtr->genAndScoreMoves(mBoard, false, ttMove, mMovesHistory);

        assert(!singular || plyDataPtr->mCurrentMoveIdx == 0);

        u64 pinned = mBoard.pinned();
        int legalMovesSeen = 0;

        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        ArrayVec<Move, 256> failLowQuiets;

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

            // Moves loop pruning
            if (ply > 0 && bestScore > -MIN_MATE_SCORE && moveScore < KILLER_SCORE)
            {
                i32 lmrDepth = std::max(0, depth - LMR_TABLE[depth][legalMovesSeen]);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !mBoard.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + lmrDepth * fpMultiplier())
                    break;

                // SEE pruning
                i32 threshold = depth * (isQuiet ? seeQuietThreshold() : seeNoisyThreshold());
                if (depth <= seePruningMaxDepth() && !SEE(mBoard, move, threshold))
                    continue;
            }

            i32 newDepth = depth - 1;

            // SE (Singular extensions)
            // In singular searches, ttMove = MOVE_NONE, which prevents SE

            i32 singularBeta;

            if (move == ttMove
            && !mBoard.inCheck()
            && ply > 0
            && depth >= singularMinDepth()
            && (i32)ttEntry.depth >= depth - singularDepthMargin()
            && ttEntry.bound() != Bound::UPPER 
            && abs(ttEntry.score) < MIN_MATE_SCORE
            && (singularBeta = (i32)ttEntry.score - depth * singularBetaMultiplier()) > -MIN_MATE_SCORE + 1)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                            doubleExtsLeft, true);

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
                    
                plyDataPtr->mCurrentMoveIdx = 0; // reset since the singular search used this
            }

            u64 nodesBefore = mNodes;
            makeMove(move, plyDataPtr);

            i32 score = 0, lmr = 0;

            if (mBoard.isDraw(ply)) goto moveSearched;

            newDepth += mBoard.inCheck(); // Check extension

            // PVS (Principal variation search)

            // First move, aka left-most leaf, is a PV node searched with full window
            if (legalMovesSeen == 1) {
                score = -search(newDepth, ply + 1, -beta, -alpha, doubleExtsLeft);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 2 && !mBoard.inCheck() && legalMovesSeen >= lmrMinMoves() && moveScore < KILLER_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesSeen];
                lmr -= pvNode; // reduce pv nodes less

                if (lmr < 0) lmr = 0; // dont extend
            }

            // Reduced, zero window search (non PV node)
            score = -search(newDepth - lmr, ply + 1, -alpha - 1, -alpha, doubleExtsLeft);

            // Research (PV node)
            if (score > alpha && (score < beta || lmr > 0))
                score = -search(newDepth, ply + 1, -beta, -alpha, doubleExtsLeft);

            moveSearched:

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            assert(mNodes > nodesBefore);
            if (ply == 0) mMovesNodes[move.encoded()] += mNodes - nodesBefore;
            
            if (score > bestScore) bestScore = score;

            // Fail low?
            if (score <= alpha) {
                if (isQuiet) failLowQuiets.push_back(move);
                
                continue;
            }

            alpha = score;
            bestMove = move;
            bound = Bound::EXACT;

            if (pvNode) plyDataPtr->updatePV(move);

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            if (!isQuiet) break;

            // This move is a fail high quiet

            plyDataPtr->mKiller = move;

            int stm = (int)mBoard.sideToMove();
            int pt = (int)move.pieceType();
            i32 bonus = depth * depth;
            
            // History bonus: increase this move's history
            mMovesHistory[stm][pt][move.to()].update(
                bonus * historyBonusMultiplier(), 
                plyDataPtr->mEnemyAttacks & bitboard(move.from()), 
                plyDataPtr->mEnemyAttacks & bitboard(move.to()), 
                mBoard.lastMove());

            // History malus: decrease history of fail low quiets
            for (Move failLow : failLowQuiets) {
                pt = (int)failLow.pieceType();

                mMovesHistory[stm][pt][failLow.to()].update(
                    -bonus * historyMalusMultiplier(),
                    plyDataPtr->mEnemyAttacks & bitboard(failLow.from()), 
                    plyDataPtr->mEnemyAttacks & bitboard(failLow.to()),  
                    mBoard.lastMove());
            }

            break;
        }

        if (legalMovesSeen == 0) 
            return singular ? alpha 
                   : mBoard.inCheck() ? -INF + ply // checkmate
                   : 0; // stalemate

        // Store in TT
        if (!singular)
            (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), depth, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    inline i32 qSearch(i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        if (ply > mMaxPlyReached) mMaxPlyReached = ply; // update seldepth

        PlyData* plyDataPtr = &mPliesData[ply];

        // Max ply cutoff
        if (ply >= mMaxDepth)
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        if (eval >= beta) return eval; 
        if (eval > alpha) alpha = eval;

        // generate noisy moves except underpromotions
        plyDataPtr->genAndScoreMoves(mBoard, true, MOVE_NONE, mMovesHistory);

        u64 pinned = mBoard.pinned();
        i32 bestScore = mBoard.inCheck() ? -INF + ply : eval;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove())
        {
            assert(mBoard.captured(move) != PieceType::NONE || move.promotion() != PieceType::NONE);

            // SEE pruning (skip bad noisy moves)
            if (moveScore < 0) break;

            // skip illegal moves
            if (!mBoard.isPseudolegalLegal(move, pinned)) continue;

            makeMove(move, plyDataPtr);

            i32 score = mBoard.isDraw(ply) ? 0 : -qSearch(ply + 1, -beta, -alpha);

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            if (score <= bestScore) continue;

            bestScore = score;

            if (bestScore >= beta) break; // Fail high / beta cutoff
            
            if (bestScore > alpha) alpha = bestScore;
        }

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