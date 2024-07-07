// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256;

#include "board.hpp"
#include "search_params.hpp"
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
        mMaxNodes = I64_MAX;

    u8 mMaxDepth = MAX_DEPTH;
    u64 mNodes = 0;
    u8 mMaxPlyReached = 0; // seldepth

    std::array<PlyData, MAX_DEPTH+1> mPliesData;

    std::array<Accumulator, MAX_DEPTH+1> mAccumulators;
    Accumulator* mAccumulatorPtr = &mAccumulators[0];

    std::vector<TTEntry>* ttPtr = nullptr;
    
    public:

    inline static bool sSearchStopped = false; // This must be reset to false before calling search()

    inline SearchThread(std::vector<TTEntry>* ttPtr) {
        this->ttPtr = ttPtr;
    }

    inline void reset() {

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

    inline i32 search(Board &board, i32 maxDepth, auto startTime, i64 hardMilliseconds, i64 maxNodes)
    { 
        mBoard = board;
        mStartTime = startTime;
        mHardMilliseconds = std::max(hardMilliseconds, (i64)0);
        mMaxDepth = std::clamp(maxDepth, 1, (i32)MAX_DEPTH);
        mMaxNodes = std::max(maxNodes, (i64)0);

        mPliesData[0] = PlyData();

        mAccumulators[0] = Accumulator(mBoard);
        mAccumulatorPtr = &mAccumulators[0];

        mNodes = 0;

        i32 score = 0;

        // ID (Iterative deepening)
        for (i32 iterationDepth = 1; iterationDepth <= mMaxDepth; iterationDepth++)
        {
            mMaxPlyReached = 0;
            Move moveBefore = bestMoveRoot();

            i32 iterationScore = search(iterationDepth, 0, -INF, INF);

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
            eval = evaluate(mAccumulatorPtr, mBoard, false);
            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

    inline i32 search(i32 depth, i32 ply, i32 alpha, i32 beta) {
        assert(ply >= 0 && ply <= mMaxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta  >= -INF && beta  <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (ply > mMaxPlyReached) mMaxPlyReached = ply; // update seldepth

        PlyData* plyDataPtr = &mPliesData[ply];

        if (depth > mMaxDepth) depth = mMaxDepth;

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(mBoard.zobristHash(), ttPtr->size());
        TTEntry ttEntry = (*ttPtr)[ttEntryIdx];
        bool ttHit = mBoard.zobristHash() == ttEntry.zobristHash && ttEntry.bound() != Bound::NONE;

        // TT cutoff (not done in singular searches since ttHit is false)
        if (ttHit 
        && ply > 0
        && ttEntry.depth >= depth 
        && (ttEntry.bound() == Bound::EXACT
        || (ttEntry.bound() == Bound::LOWER && ttEntry.score >= beta) 
        || (ttEntry.bound() == Bound::UPPER && ttEntry.score <= alpha)))
            return ttEntry.adjustedScore(ply);

        if (ply >= mMaxDepth) 
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        if (!mAccumulatorPtr->mUpdated) 
            mAccumulatorPtr->update(mAccumulatorPtr - 1, mBoard);

        Move ttMove = ttHit ? Move(ttEntry.move) : MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() && ttMove == MOVE_NONE)
            depth--;

        plyDataPtr->genAndScoreMoves(mBoard, false, ttMove);

        u64 pinned = mBoard.pinned();
        int legalMovesSeen = 0;

        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Moves loop
        for (auto [move, moveScore] = plyDataPtr->nextMove(); 
             move != MOVE_NONE; 
             std::tie(move, moveScore) = plyDataPtr->nextMove())
        {
            // skip illegal moves
            if (!mBoard.isPseudolegalLegal(move, pinned)) continue;

            legalMovesSeen++;

            makeMove(move, plyDataPtr);

            i32 score = mBoard.isRepetition(ply) ? 0 : -search(depth - 1 + mBoard.inCheck(), ply + 1, -beta, -alpha);

            mBoard.undoMove();
            mAccumulatorPtr--;

            if (stopSearch()) return 0;

            if (score > bestScore) bestScore = score;

            // Fail low?
            if (score <= alpha) continue;

            alpha = score;
            bestMove = move;
            bound = Bound::EXACT;

            plyDataPtr->mPvLine.clear();
            plyDataPtr->mPvLine.push_back(move);

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            break;
        }

        if (legalMovesSeen == 0) 
            // checkmate or stalemate
            return mBoard.inCheck() ? -INF + ply : 0;

        // Store in TT
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

        if (ply >= mMaxDepth)
            return mBoard.inCheck() ? 0 : updateAccumulatorAndEval(plyDataPtr->mEval);

        i32 eval = updateAccumulatorAndEval(plyDataPtr->mEval);

        if (eval >= beta) return eval; 
        if (eval > alpha) alpha = eval;

        // generate noisy moves except underpromotions
        plyDataPtr->genAndScoreMoves(mBoard, true, MOVE_NONE);

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

            i32 score = -qSearch(ply + 1, -beta, -alpha);

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