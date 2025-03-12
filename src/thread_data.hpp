// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search_params.hpp"
#include "nnue.hpp"
#include "history_entry.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>

struct PlyData
{
public:

    ArrayVec<Move, MAX_DEPTH + 1> pvLine;
    std::optional<i32> eval = std::nullopt;
    Move killer = MOVE_NONE;
};

enum class ThreadState : i32 {
    Sleeping, Searching, ExitAsap, Exited
};

struct ThreadData
{
public:

    Position pos;

    i32 rootDepth = 0;

    std::atomic<u64> nodes = 0;
    size_t maxPlyReached = 0;

    std::array<PlyData, MAX_DEPTH + 1> pliesData; // [ply]

    std::array<u64, 1ULL << 17> nodesByMove; // [Move.asU16()]

    // [stm][pieceType][targetSquare]
    HistoryTable historyTable = { };

    std::array<nnue::BothAccumulators, MAX_DEPTH + 1> bothAccsStack;
    size_t bothAccsIdx = 0;

    nnue::FinnyTable finnyTable; // [color][mirrorVAxis][inputBucket]

    // [stm][Position.pawnsHash() % CORR_HIST_SIZE]
    EnumArray<std::array<i16, CORR_HIST_SIZE>, Color> pawnsCorrHist = { };

    // [stm][pieceColor][[Position.nonPawnsHash(pieceColor) % CORR_HIST_SIZE]
    EnumArray<std::array<i16, CORR_HIST_SIZE>, Color, Color> nonPawnsCorrHist = { };

    // Threading stuff
    ThreadState threadState = ThreadState::Sleeping;
    std::mutex mutex;
    std::condition_variable cv;

}; // struct ThreadData

inline void wakeThread(ThreadData* td, const ThreadState newState)
{
    std::unique_lock<std::mutex> lock(td->mutex);
    td->threadState = newState;
    lock.unlock();
    td->cv.notify_one();
}

constexpr Move bestMoveAtRoot(const ThreadData* td)
{
    return td->pliesData[0].pvLine.size() > 0
         ? td->pliesData[0].pvLine[0]
         : MOVE_NONE;
}

constexpr void updateBothAccs(ThreadData* td)
{
    assert(td->bothAccsIdx > 0 || td->bothAccsStack[td->bothAccsIdx].mUpdated);

    if (td->bothAccsIdx > 0)
    {
        td->bothAccsStack[td->bothAccsIdx]
            .updateMove(td->bothAccsStack[td->bothAccsIdx - 1], td->pos, td->finnyTable);
    }
}

constexpr std::array<i16*, 4> getCorrHists(ThreadData* td)
{
    const size_t whiteNonPawnsIdx = td->pos.nonPawnsHash(Color::White) % CORR_HIST_SIZE;
    const size_t blackNonPawnsIdx = td->pos.nonPawnsHash(Color::Black) % CORR_HIST_SIZE;

    i16* lastMoveCorrHistPtr = nullptr;
    const Move lastMove = td->pos.lastMove();

    if (lastMove != MOVE_NONE)
    {
        HistoryEntry& historyEntry
            = td->historyTable[td->pos.sideToMove()][lastMove.pieceType()][lastMove.to()];

        lastMoveCorrHistPtr = &(historyEntry.mCorrHist);
    }

    return {
        &(td->pawnsCorrHist[td->pos.sideToMove()][td->pos.pawnsHash() % CORR_HIST_SIZE]),
        &(td->nonPawnsCorrHist[td->pos.sideToMove()][Color::White][whiteNonPawnsIdx]),
        &(td->nonPawnsCorrHist[td->pos.sideToMove()][Color::Black][blackNonPawnsIdx]),
        lastMoveCorrHistPtr
    };
}

constexpr i32 getEval(ThreadData* td, const size_t ply)
{
    std::optional<i32>& eval = td->pliesData[ply].eval;

    if (td->pos.inCheck())
        eval = 0;
    else if (!eval)
    {
        updateBothAccs(td);

        eval = nnue::evaluate(td->bothAccsStack[td->bothAccsIdx], td->pos.sideToMove());

        // Adjust eval with correction histories

        const auto [pawnsCorr, whiteNonPawnsCorr, blackNonPawnsCorr, lastMoveCorr]
            = getCorrHists(td);

        const i32 nonPawnsCorr
            = static_cast<i32>(*whiteNonPawnsCorr) + static_cast<i32>(*blackNonPawnsCorr);

        float correction = static_cast<float>(*pawnsCorr)   * corrHistPawnsWeight()
                         + static_cast<float>(nonPawnsCorr) * corrHistNonPawnsWeight();

        if (lastMoveCorr != nullptr)
            correction += static_cast<float>(*lastMoveCorr) * corrHistLastMoveWeight();

        *eval += static_cast<i32>(round(correction));

        // Clamp eval to avoid invalid values and checkmate values
        eval = std::clamp<i32>(*eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
    }

    return *eval;
}

constexpr GameState makeMove(ThreadData* td, const Move move, const size_t newPly)
{
    td->pos.makeMove(move);
    td->nodes.fetch_add(1, std::memory_order_relaxed);

    // Update seldepth
    td->maxPlyReached = std::max<size_t>(td->maxPlyReached, newPly);

    PlyData& newPlyData = td->pliesData[newPly];
    newPlyData.pvLine.clear();
    newPlyData.eval = std::nullopt;

    if (move != MOVE_NONE)
    {
        td->bothAccsIdx++;
        td->bothAccsStack[td->bothAccsIdx].mUpdated = false;
    }

    return td->pos.gameState(hasLegalMove, newPly);
}

constexpr void undoMove(ThreadData* td)
{
    if (td->pos.lastMove() != MOVE_NONE)
        td->bothAccsIdx--;

    td->pos.undoMove();
}
