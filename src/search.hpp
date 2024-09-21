// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256,
              VALUE_NONE = INF + 1;

#include "board.hpp"
#include "search_params.hpp"
#include "history_entry.hpp"
#include "move_picker.hpp"
#include "tt.hpp"
#include "nnue.hpp"

inline u64 totalNodes();

MultiArray<i32, 2, MAX_DEPTH+1, 256> LMR_TABLE = {}; // [isQuietMove][depth][moveIndex]

inline void initLmrTable()
{
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

struct PlyData {
    public:
    ArrayVec<Move, MAX_DEPTH+1> pvLine = {};
    Move killer = MOVE_NONE;
    i32 eval = VALUE_NONE;
};

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
        mMovesHistory     = {};
        mPawnsCorrHist    = {};
        mNonPawnsCorrHist = {};
        mTTAge = 0;
    }

    inline u64 getNodes() const { return mNodes; }

    inline u64 millisecondsElapsed() const {
        return (std::chrono::steady_clock::now() - mStartTime) / std::chrono::milliseconds(1);
    }

    inline Move bestMoveRoot() const {
        return mPliesData[0].pvLine.size() == 0 
               ? MOVE_NONE : mPliesData[0].pvLine[0];
    }

    inline i32 search(const Board &board, const i32 maxDepth, const auto startTime, 
        const i64 hardMilliseconds, const i64 softMilliseconds, const i64 maxNodes)
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

            const i32 iterationScore = iterationDepth >= aspMinDepth() 
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
                const i32 movesTillMate = round((INF - abs(score)) / 2.0);
                std::cout << " score mate " << (score > 0 ? movesTillMate : -movesTillMate);
            }

            const u64 nodes = totalNodes();
            const u64 msElapsed = millisecondsElapsed();

            std::cout << " nodes " << nodes
                      << " nps "   << nodes * 1000 / std::max(msElapsed, (u64)1)
                      << " time "  << msElapsed
                      << " pv";

            for (const Move move : mPliesData[0].pvLine)
                std::cout << " " << move.toUci();

            std::cout << std::endl;

            // Check soft time limit (in case one exists)

            if (mSoftMilliseconds == I64_MAX) continue;

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

    inline std::array<i16*, 4> correctionHistories()
    {
        const int stm = (int)mBoard.sideToMove();
        i16* lastMoveCorrHistPtr = nullptr;

        if (mBoard.lastMove() != MOVE_NONE) {
            const PieceType pt = mBoard.lastMove().pieceType();
            lastMoveCorrHistPtr = &(mMovesHistory[stm][(int)pt][mBoard.lastMove().to()].mCorrHist);
        }

        return {
            &mPawnsCorrHist[stm][mBoard.pawnsHash() % 16384],
            &mNonPawnsCorrHist[stm][WHITE][mBoard.nonPawnsHash(Color::WHITE) % 16384],
            &mNonPawnsCorrHist[stm][BLACK][mBoard.nonPawnsHash(Color::BLACK) % 16384],
            lastMoveCorrHistPtr
        };
    }

    inline void makeMove(const Move move, const i32 newPly)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after it
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) {
            const auto ttEntryIdx = TTEntryIndex(mBoard.roughHashAfter(move), ttPtr->size());
            __builtin_prefetch(&(*ttPtr)[ttEntryIdx]);
        }

        mBoard.makeMove(move);
        mNodes++;

        // update seldepth
        if (newPly > mMaxPlyReached) mMaxPlyReached = newPly;

        mPliesData[newPly].pvLine.clear();
        mPliesData[newPly].eval = VALUE_NONE;

        // Killer move must be quiet move
        if (mPliesData[newPly].killer != MOVE_NONE && !mBoard.isQuiet(mPliesData[newPly].killer))
            mPliesData[newPly].killer = MOVE_NONE;
        
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

            // Correct eval with correction histories

            i32 correction = 0;

            for (const i16* corrHist : correctionHistories())
                if (corrHist != nullptr) 
                    correction += i32(*corrHist);

            eval += correction / corrHistScale();

            // Clamp to avoid false mate scores and invalid scores
            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

    inline i32 aspiration(const i32 iterationDepth, i32 score)
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

    inline i32 search(i32 depth, const i32 ply, i32 alpha, i32 beta, 
        const bool cutNode, u8 doubleExtsLeft, const Move singularMove = MOVE_NONE) 
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
        const auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = singularMove != MOVE_NONE ? TTEntry() : (*ttPtr)[ttEntryIdx];
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
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = updateAccumulatorAndEval(plyDataPtr->eval);

        const int improving = ply <= 1 || mBoard.inCheck() || mBoard.inCheck2PliesAgo()
                              ? 0
                              : eval - (plyDataPtr - 2)->eval >= improvingThreshold()
                              ? 1
                              : eval - (plyDataPtr - 2)->eval <= -improvingThreshold()
                              ? -1
                              : 0;

        (plyDataPtr + 1)->killer = MOVE_NONE;

        // Node pruning
        if (!pvNode && !mBoard.inCheck() && singularMove == MOVE_NONE) 
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
                const i32 score = qSearch(ply, alpha, beta);

                if (stopSearch()) return 0;

                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth() 
            && mBoard.lastMove() != MOVE_NONE 
            && eval >= beta
            && eval >= beta + nmpEvalBetaMargin() - depth * nmpEvalBetaMultiplier()
            && !(ttHit && ttEntry.bound() == Bound::UPPER && ttEntry.score < beta)
            && mBoard.hasNonPawnMaterial(mBoard.sideToMove()))
            {
                makeMove(MOVE_NONE, ply + 1);

                const i32 nmpDepth = depth - nmpBaseReduction() - depth / nmpReductionDivisor();
                
                const i32 score = mBoard.isDraw(ply + 1) ? 0 
                                  : -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtsLeft);

                mBoard.undoMove();

                if (stopSearch()) return 0;

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

                if (stopSearch()) return 0;

                if (probcutScore != VALUE_NONE) return probcutScore;
            }
        }

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() 
        && (ttMove == MOVE_NONE || ttEntry.depth() < depth - iirMinDepth()) 
        && singularMove == MOVE_NONE 
        && (pvNode || cutNode))
            depth--;

        const bool lmrImproving = ply > 1 && !mBoard.inCheck() && !mBoard.inCheck2PliesAgo() 
                                  && eval > (plyDataPtr - 2)->eval;

        const int stm = (int)mBoard.sideToMove();
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
            const bool isQuiet = mBoard.isQuiet(move);

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

            // Moves loop pruning
            if (ply > 0 
            && bestScore > -MIN_MATE_SCORE 
            && legalMovesSeen >= 3 
            && (movePicker.stage() == MoveGenStage::QUIETS || movePicker.stage() == MoveGenStage::BAD_NOISIES))
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + mBoard.inCheck()
                                      + depth * depth * lmpMultiplier())
                    break;
                    
                const i32 lmrDepth = depth - LMR_TABLE[isQuiet][depth][legalMovesSeen];

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !mBoard.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + std::max(lmrDepth + improving, 0) * fpMultiplier())
                    break;

                // SEE pruning
                const i32 threshold = depth * (isQuiet ? seeQuietThreshold() : seeNoisyThreshold());
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

            const PieceType captured = mBoard.captured(move);

            const u64 nodesBefore = mNodes;
            makeMove(move, ply + 1);

            int pt = (int)move.pieceType();
            HistoryEntry &historyEntry = mMovesHistory[stm][pt][move.to()];

            i32 score = 0;

            if (mBoard.isDraw(ply + 1)) 
                goto moveSearched;

            // PVS (Principal variation search)

            // LMR (Late move reductions)
            if (depth >= 2 
            && !mBoard.inCheck() 
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
                            : float(historyEntry.noisyHistory(captured)) / (float)lmrNoisyHistoryDiv()
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

            const i32 bonus =  std::clamp(depth * historyBonusMultiplier() - historyBonusOffset(), 0, historyBonusMax());
            const i32 malus = -std::clamp(depth * historyMalusMultiplier() - historyMalusOffset(), 0, historyMalusMax());

            // History malus: decrease history of fail low noisy moves
            for (i16* noisyHistoryPtr : failLowNoisiesHistory)
                updateHistory(noisyHistoryPtr, malus);

            if (!isQuiet) {
                // History bonus: increase this move's history
                updateHistory(historyEntry.noisyHistoryPtr(captured), bonus);
                break;
            }

            // This move is a fail high quiet

            plyDataPtr->killer = move;
            const Color nstm = mBoard.oppSide();

            if (failLowQuiets.size() > 0)
                // Calling attacks(nstm) will cache enemy attacks and speedup isSquareAttacked()
                mBoard.attacks(nstm);

            const std::array<Move, 3> lastMoves = { 
                mBoard.lastMove(), mBoard.nthToLastMove(2), mBoard.nthToLastMove(4) 
            };

            // History bonus: increase this move's history
            historyEntry.updateQuietHistories(
                mBoard.isSquareAttacked(move.from(), nstm), 
                mBoard.isSquareAttacked(move.to(),   nstm),
                lastMoves, 
                bonus
            );

            // History malus: decrease history of fail low quiets
            for (const Move failLow : failLowQuiets) {
                pt = (int)failLow.pieceType();

                mMovesHistory[stm][pt][failLow.to()].updateQuietHistories(
                    mBoard.isSquareAttacked(failLow.from(), nstm),
                    mBoard.isSquareAttacked(failLow.to(),   nstm),
                    lastMoves,
                    malus
                );
            }

            break;
        }

        if (legalMovesSeen == 0) {
            if (singularMove != MOVE_NONE) return alpha;

            assert(!mBoard.hasLegalMove());

            // Checkmate or stalemate
            return mBoard.inCheck() ? -INF + ply : 0;
        }

        assert(mBoard.hasLegalMove());

        if (singularMove == MOVE_NONE) {
            // Store in TT
            (*ttPtr)[ttEntryIdx].update(mBoard.zobristHash(), depth, ply, bestScore, bestMove, bound, mTTAge);

            // Update correction histories
            if (!mBoard.inCheck()
            && abs(bestScore) < MIN_MATE_SCORE
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                const i32 newWeight = std::min(depth + depth * depth, corrHistScale());

                for (i16* corrHist : correctionHistories())
                {
                    if (corrHist == nullptr) continue;

                    i32 newValue = *corrHist;
                    newValue *= std::max(corrHistScale() - newWeight, 1);
                    newValue += (bestScore - eval) * corrHistScale() * newWeight;
                    *corrHist = std::clamp(newValue / corrHistScale(), -corrHistMax(), corrHistMax());
                }
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

        if (stopSearch()) return 0;

        // Probe TT
        const auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = (*ttPtr)[ttEntryIdx];
        const bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash;
        Move ttMove = MOVE_NONE;

        // TT cutoff
        if (ttHit) {
            ttEntry.adjustScore(ply);
            ttMove = Move(ttEntry.move);

            if (ttEntry.bound() == Bound::EXACT
            || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
            || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha))
                return ttEntry.score;
        }

        PlyData* plyDataPtr = &mPliesData[ply];

        // Max ply cutoff
        if (ply >= mMaxDepth)
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->eval);

        const i32 eval = updateAccumulatorAndEval(plyDataPtr->eval);

        if (!mBoard.inCheck()) {
            if (eval >= beta) return eval; 
            if (eval > alpha) alpha = eval;
        }
        
        i32 bestScore = mBoard.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop

        MovePicker movePicker = MovePicker(!mBoard.inCheck());
        Move move;

        while ((move = movePicker.next(mBoard, ttMove, plyDataPtr->killer, mMovesHistory)) != MOVE_NONE)
        {
            assert([&]() {
                const MoveGenStage stage = movePicker.stage();
                const bool isNoisiesStage = stage == MoveGenStage::GOOD_NOISIES || stage == MoveGenStage::BAD_NOISIES;
                const bool isQuiet = mBoard.isQuiet(move);

                if (stage == MoveGenStage::TT_MOVE_YIELDED)
                    return move == ttMove;

                if (mBoard.inCheck())
                    return stage == MoveGenStage::KILLER_YIELDED 
                           ? move == plyDataPtr->killer && isQuiet
                           : (isQuiet ? stage == MoveGenStage::QUIETS : isNoisiesStage);

                return isNoisiesStage && !isQuiet;
            }());

            // If in check, skip quiets and bad noisy moves
            if (mBoard.inCheck() && bestScore > -MIN_MATE_SCORE && int(movePicker.stage()) > int(MoveGenStage::GOOD_NOISIES))
                break;

            // If not in check, skip bad noisy moves
            if (!mBoard.inCheck() && !mBoard.SEE(move))
                continue;
                
            makeMove(move, ply + 1);

            const i32 score = mBoard.isDraw(ply + 1) ? 0 : -qSearch(ply + 1, -beta, -alpha);

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

            assert(!mBoard.isQuiet(move));

            // SEE pruning (skip bad noisy moves)
            if (!mBoard.SEE(move, probcutBeta - plyDataPtr->eval)) 
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
