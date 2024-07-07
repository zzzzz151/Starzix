// clang-format off

#pragma once

constexpr i32 INF = 32000, 
              MIN_MATE_SCORE = INF - 256;

#include "board.hpp"
#include "search_params.hpp"
#include "ply_data.hpp"
#include "tt.hpp"
#include "see.hpp"
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

    inline Move bestMoveRoot() {
        return mPliesData[0].mPvLine.size() == 0 
               ? MOVE_NONE : mPliesData[0].mPvLine[0];
    }

    inline u64 getNodes() { return mNodes; }

    inline u64 millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - mStartTime) / std::chrono::milliseconds(1);
    }

    inline i32 search(Board &board, i32 maxDepth, auto startTime, i64 hardMilliseconds, i64 maxNodes)
    { 
        resetRng();

        mBoard = board;
        mStartTime = startTime;
        mHardMilliseconds = std::max(hardMilliseconds, (i64)0);
        mMaxDepth = std::clamp(maxDepth, 1, (i32)MAX_DEPTH);
        mMaxNodes = std::max(maxNodes, (i64)0);

        mPliesData[0] = PlyData();
        mNodes = 0;

        ArrayVec<Move, 256> moves;
        board.pseudolegalMoves(moves, false, false);

        for (std::size_t i = 0; i < moves.mSize; i++)
            moves.swap(i, randomU64() % (u64)moves.mSize);

        u64 pinned = board.pinned();

        for (Move move : moves)
            if (board.isPseudolegalLegal(move, pinned))
            {
                mPliesData[0].mPvLine.push_back(move);
                return 0;
            }

        return 0;
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

}; // class SearchThread

// in main.cpp, main thread is created and gMainThread = &gSearchThreads[0]
std::vector<SearchThread> gSearchThreads;

inline u64 totalNodes() {
    u64 nodes = 0;

    for (SearchThread &searchThread : gSearchThreads)
        nodes += searchThread.getNodes();

    return nodes;
}