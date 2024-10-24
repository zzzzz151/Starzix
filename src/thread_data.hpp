// clang-format off

#pragma once 

#include "utils.hpp"
#include "board.hpp"
#include "search_params.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "nnue.hpp"
#include <mutex>
#include <condition_variable>

struct PlyData {
    public:
    ArrayVec<Move, MAX_DEPTH+1> pvLine = { };
    Move killer = MOVE_NONE;
    i32 eval = VALUE_NONE;
};

enum class ThreadState {
    SLEEPING, SEARCHING, EXIT_ASAP, EXITED
};

struct ThreadData {
    public:

    Board board = START_BOARD;

    i32 score = 0;

    u64 nodes = 0;
    i32 maxPlyReached = 0;

    std::array<PlyData, MAX_DEPTH+1> pliesData; // [ply]

    // [stm][pieceType][targetSquare]
    MultiArray<HistoryEntry, 2, 6, 64> historyTable = { }; 

    std::array<u64, 1ULL << 17> nodesByMove; // [move]

    std::array<BothAccumulators, MAX_DEPTH+1> accumulators;
    BothAccumulators* accumulatorPtr = &accumulators[0];

    FinnyTable finnyTable; // [color][mirrorHorizontally][inputBucket]

    // [stm][board.pawnsHash() % CORR_HIST_SIZE]
    MultiArray<i16, 2, CORR_HIST_SIZE> pawnsCorrHist = { }; 

    // [stm][pieceColor][board.nonPawnsHash(pieceColor) % CORR_HIST_SIZE]
    MultiArray<i16, 2, 2, CORR_HIST_SIZE> nonPawnsCorrHist = { }; 

    // Threading stuff
    ThreadState threadState = ThreadState::SLEEPING;
    std::mutex mutex;
    std::condition_variable cv;

    inline ThreadData() = default;

    inline void wake(const ThreadState newState) 
    {
        std::unique_lock<std::mutex> lock(mutex);
        threadState = newState;
        lock.unlock();
        cv.notify_one();
    }

    inline std::array<i16*, 4> correctionHistories()
    {
        const int stm = int(board.sideToMove());

        const Move lastMove = board.lastMove();
        i16* lastMoveCorrHistPtr = nullptr;

        if (lastMove != MOVE_NONE) {
            const int pt = int(lastMove.pieceType());
            lastMoveCorrHistPtr = &(historyTable[stm][pt][lastMove.to()].mCorrHist);
        }

        return {
            &pawnsCorrHist[stm][board.pawnsHash() % CORR_HIST_SIZE],
            &nonPawnsCorrHist[stm][WHITE][board.nonPawnsHash(Color::WHITE) % CORR_HIST_SIZE],
            &nonPawnsCorrHist[stm][BLACK][board.nonPawnsHash(Color::BLACK) % CORR_HIST_SIZE],
            lastMoveCorrHistPtr
        };
    }

    inline void makeMove(const Move move, const i32 newPly, const std::vector<TTEntry> &tt)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after it
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) {
            const auto ttEntryIdx = TTEntryIndex(board.roughHashAfter(move), tt.size());
            __builtin_prefetch(&tt[ttEntryIdx]);
        }

        board.makeMove(move);
        nodes++;

        // update seldepth
        if (newPly > maxPlyReached) maxPlyReached = newPly;

        pliesData[newPly].pvLine.clear();
        pliesData[newPly].eval = VALUE_NONE;

        // Killer move must be quiet move
        if (pliesData[newPly].killer != MOVE_NONE && !board.isQuiet(pliesData[newPly].killer))
            pliesData[newPly].killer = MOVE_NONE;
        
        if (move != MOVE_NONE) {
            accumulatorPtr++;
            accumulatorPtr->mUpdated = false;
        }
    }

    inline i32 updateAccumulatorAndEval(i32 &eval) 
    {
        assert(accumulatorPtr == &accumulators[0] 
               ? accumulatorPtr->mUpdated 
               : (accumulatorPtr - 1)->mUpdated);

        if (!accumulatorPtr->mUpdated) 
            accumulatorPtr->update(accumulatorPtr - 1, board, finnyTable);

        if (board.inCheck())
            eval = 0;
        else if (eval == VALUE_NONE) 
        {
            assert(BothAccumulators(board) == *accumulatorPtr);

            eval = nnue::evaluate(accumulatorPtr, board.sideToMove());

            eval *= materialScale(board); // Scale eval with material

            // Correct eval with correction histories

            #if defined(TUNE)
                CORR_HISTS_WEIGHTS = {
                    corrHistPawnsWeight(), 
                    corrHistNonPawnsWeight(), 
                    corrHistNonPawnsWeight(), 
                    corrHistLastMoveWeight()
                };
            #endif

            const auto corrHists = correctionHistories();

            for (size_t i = 0; i < corrHists.size() - (corrHists.back() == nullptr); i++)
            {
                const float corrHist = *(corrHists[i]);
                eval += corrHist * CORR_HISTS_WEIGHTS[i];
            }

            // Clamp to avoid false mate scores and invalid scores
            eval = std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return eval;
    }

}; // struct ThreadData
