// clang-format off

#pragma once

#include "utils.hpp"
#include "position.hpp"
#include "move_gen.hpp"
#include "search_params.hpp"
#include "nnue.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>

struct PlyData
{
public:

    bool inCheck;
    ArrayVec<Move, MAX_DEPTH + 1> pvLine;
    Move killer = MOVE_NONE;

    std::optional<i32> rawEval       = std::nullopt;
    std::optional<i32> correctedEval = std::nullopt;

    ArrayVec<Move, 256> failLowQuiets;
    ArrayVec<std::pair<Move, std::optional<PieceType>>, 256> failLowNoisies; // move, captured

}; // struct PlyData

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

    HistoryTable historyTable = { };

    std::array<nnue::BothAccumulators, MAX_DEPTH + 1> bothAccsStack;
    size_t bothAccsIdx = 0;

    nnue::FinnyTable finnyTable;

    // [stm][pawnsHash % CORR_HIST_SIZE]
    EnumArray<std::array<i16, CORR_HIST_SIZE>, Color> pawnsCorrHist = { };

    // [stm][pieceColor][[pieceColorNonPawnsHash % CORR_HIST_SIZE]
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

constexpr void makeMove(
    ThreadData* td, const Move move, const size_t newPly, std::vector<TTEntry>* ttPtr = nullptr)
{
    td->pos.makeMove(move);

    // Prefetch TT entry
    if (ttPtr != nullptr)
    {
        const TTEntry& ttEntry = ttEntryRef(*ttPtr, td->pos.zobristHash());
        __builtin_prefetch(&ttEntry);
    }

    td->nodes.fetch_add(1, std::memory_order_relaxed);

    // Update seldepth
    td->maxPlyReached = std::max<size_t>(td->maxPlyReached, newPly);

    PlyData& newPlyData = td->pliesData[newPly];
    newPlyData.inCheck = td->pos.inCheck();
    newPlyData.pvLine.clear();
    newPlyData.rawEval = newPlyData.correctedEval = std::nullopt;

    if (move)
    {
        td->bothAccsIdx++;
        td->bothAccsStack[td->bothAccsIdx].mUpdated = false;
    }
}

constexpr void undoMove(ThreadData* td)
{
    if (td->pos.lastMove())
        td->bothAccsIdx--;

    td->pos.undoMove();
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

constexpr auto corrHistsPtrs(ThreadData* td)
{
    const size_t whiteNonPawnsIdx = td->pos.nonPawnsHash(Color::White) % CORR_HIST_SIZE;
    const size_t blackNonPawnsIdx = td->pos.nonPawnsHash(Color::Black) % CORR_HIST_SIZE;

    Move prevMove = td->pos.lastMove();
    i16* lastMoveCorrPtr = nullptr;
    i16* contCorrPtr     = nullptr;

    if (prevMove)
    {
        HistoryEntry& histEntry
            = td->historyTable[td->pos.sideToMove()][prevMove.pieceType()][prevMove.to()];

        lastMoveCorrPtr = &(histEntry.mCorrHist);

        prevMove = td->pos.nthToLastMove(2);

        if (prevMove)
            contCorrPtr = &(histEntry.mContCorrHist[prevMove.pieceType()][prevMove.to()]);
    }

    return std::array {
        &(td->pawnsCorrHist[td->pos.sideToMove()][td->pos.pawnsHash() % CORR_HIST_SIZE]),
        &(td->nonPawnsCorrHist[td->pos.sideToMove()][Color::White][whiteNonPawnsIdx]),
        &(td->nonPawnsCorrHist[td->pos.sideToMove()][Color::Black][blackNonPawnsIdx]),
        lastMoveCorrPtr,
        contCorrPtr
    };
}

constexpr i32 getEval(ThreadData* td, PlyData& plyData)
{
    if (td->pos.inCheck())
    {
        plyData.rawEval = plyData.correctedEval = 0;
        return 0;
    }

    if (!plyData.rawEval.has_value())
    {
        updateBothAccs(td);
        plyData.rawEval = nnue::evaluate(td->bothAccsStack[td->bothAccsIdx], td->pos.sideToMove());
    }

    // Adjust eval with correction histories

    plyData.correctedEval = plyData.rawEval;

    const auto [
        pawnsCorrPtr, whiteNonPawnsCorrPtr, blackNonPawnsCorrPtr, lastMoveCorrPtr, contCorrPtr
    ] = corrHistsPtrs(td);

    const i32 nonPawnsCorr
        = static_cast<i32>(*whiteNonPawnsCorrPtr) + static_cast<i32>(*blackNonPawnsCorrPtr);

    float correction = static_cast<float>(*pawnsCorrPtr) * corrHistPawnsWeight()
                     + static_cast<float>(nonPawnsCorr)  * corrHistNonPawnsWeight();

    if (lastMoveCorrPtr != nullptr)
        correction += static_cast<float>(*lastMoveCorrPtr) * corrHistLastMoveWeight();

    if (contCorrPtr != nullptr)
        correction += static_cast<float>(*contCorrPtr) * corrHistContWeight();

    *(plyData.correctedEval) += static_cast<i32>(correction);

    // Clamp eval to avoid invalid values and checkmate values
    plyData.correctedEval = std::clamp<i32>(
        *(plyData.correctedEval), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1
    );

    return *(plyData.correctedEval);
}

constexpr void updateHistories(
    ThreadData* td,
    const Move move,
    const std::optional<PieceType> captured,
    const i32 depth,
    const i32 numFailHighs,
    const ArrayVec<std::pair<Move, std::optional<PieceType>>, 256>& noisiesToPenalize,
    const ArrayVec<Move, 256>& quietsToPenalize)
{
    HistoryEntry& histEntry
        = td->historyTable[td->pos.sideToMove()][move.pieceType()][move.to()];

    i32 histBonus = std::clamp<i32>(
        depth * histBonusMul() - histBonusOffset(), 0, histBonusMax()
    );

    histBonus *= numFailHighs;

    const i32 histMalus = -std::clamp<i32>(
        depth * histMalusMul() - histMalusOffset(), 0, histMalusMax()
    );

    // Always decrease noisy history of noisy moves to penalize
    for (const auto [move2, captured2] : noisiesToPenalize)
    {
        HistoryEntry& histEntry2
            = td->historyTable[td->pos.sideToMove()][move2.pieceType()][move2.to()];

        histEntry2.updateNoisyHistory(captured2, move2.promotion(), histMalus);
    }

    if (td->pos.isQuiet(move))
    {
        // Increase move's main hist and cont hist
        histEntry.updateMainHistContHist(td->pos, move, histBonus);

        // Decrease main hist and cont hist of quiet moves to penalize
        for (const Move move2 : quietsToPenalize)
        {
            HistoryEntry& histEntry2
                = td->historyTable[td->pos.sideToMove()][move2.pieceType()][move2.to()];

            histEntry2.updateMainHistContHist(td->pos, move2, histMalus);
        }
    }
    else {
        // Increase move's noisy history
        histEntry.updateNoisyHistory(captured, move.promotion(), histBonus);
    }
}
