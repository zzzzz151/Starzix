// clang-format off

#pragma once

#include "board.hpp"
#include "search_params.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "nnue.hpp"

inline u64 totalNodes();

constexpr u8 MAX_DEPTH = 100;

MultiArray<i32, MAX_DEPTH+1, 256> LMR_TABLE; // [depth][moveIndex]

constexpr void initLmrTable()
{
    memset(LMR_TABLE.data(), 0, sizeof(LMR_TABLE));

    for (u64 depth = 1; depth < LMR_TABLE.size(); depth++)
        for (u64 move = 1; move < LMR_TABLE[0].size(); move++)
            LMR_TABLE[depth][move] = round(lmrBase() + ln(depth) * ln(move) * lmrMultiplier());
}

struct PlyData {
    public:

    std::array<Move, MAX_DEPTH+1> pvLine = { MOVE_NONE };
    u8 pvLength = 0;
    Move killer = MOVE_NONE;
    i32 eval = INF;
    MovesList moves;
    std::array<i32, 256> movesScores;

    inline std::string pvString() {
        if (pvLength == 0) return "";

        std::string pvStr = pvLine[0].toUci();

        for (u8 i = 1; i < pvLength; i++)
            pvStr += " " + pvLine[i].toUci();

        return pvStr;
    }

    inline std::pair<Move, i32> nextMove(int idx)
    {
        // Incremental sort
        for (int j = idx + 1; j < moves.size(); j++)
            if (movesScores[j] > movesScores[idx])
            {
                moves.swap(idx, j);
                std::swap(movesScores[idx], movesScores[j]);
            }

        return { moves[idx], movesScores[idx] };
    }

}; // struct PlyData

class SearchThread;
SearchThread *mainThread = nullptr; // set to &searchThreads[0] in main()

class SearchThread {
    private:

    Board board;

    u8 maxDepth = MAX_DEPTH;
    u64 nodes = 0;
    u8 maxPlyReached = 0; // seldepth

    std::chrono::time_point<std::chrono::steady_clock> startTime;

    u64 softMilliseconds = I64_MAX, 
        hardMilliseconds = I64_MAX,
        softNodes = I64_MAX,
        hardNodes = I64_MAX;

    std::array<PlyData, MAX_DEPTH+1> pliesData = { };
    PlyData *plyDataPtr = &pliesData[0];

    std::array<Accumulator, MAX_DEPTH+1> accumulators;
    Accumulator *accumulatorPtr = &accumulators[0];

    std::array<u64, 1ULL << 17> movesNodes; // [move]
    MultiArray<Move, 2, 1ULL << 17> countermoves = {}; // [nstm][lastMove]

    // [color][pieceType][targetSquare]
    MultiArray<HistoryEntry, 2, 6, 64> historyTable = {};

    MultiArray<i32, 2, 16384> correctionHistory = {};

    std::vector<TTEntry> *tt = nullptr;

    public:

    inline static bool searchStopped = false; // This must be reset to false before calling search()

    inline SearchThread(std::vector<TTEntry> *tt) {
        this->tt = tt;
    }

    inline void reset() {
        countermoves = {};
        historyTable = {};
        correctionHistory = {};
    }

    inline Move bestMoveRoot() {
        return pliesData[0].pvLine[0];
    }

    inline u64 getNodes() { return nodes; }

    inline u64 millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - startTime) / std::chrono::milliseconds(1);
    }

    inline i32 search(Board &board, i32 maxDepth, auto startTime, i64 softMilliseconds, 
                      i64 hardMilliseconds, i64 softNodes, i64 hardNodes)
    { 
        this->board = board;
        this->maxDepth = maxDepth = std::clamp(maxDepth, 1, (i32)MAX_DEPTH);
        this->startTime = startTime;
        this->softMilliseconds = softMilliseconds = std::max(softMilliseconds, (i64)0);
        this->hardMilliseconds = hardMilliseconds = std::max(hardMilliseconds, (i64)0);
        this->softNodes = softNodes = std::max(softNodes, (i64)0);
        this->hardNodes = hardNodes = std::max(hardNodes, (i64)0);

        pliesData[0] = PlyData();
        plyDataPtr = &pliesData[0];
        
        accumulators[0] = Accumulator(board);
        accumulatorPtr = &accumulators[0];

        updateAccumulatorAndEval();

        nodes = 0;
        movesNodes = {};

        i32 score = 0;

        // ID (Iterative deepening)
        for (i32 iterationDepth = 1; iterationDepth <= this->maxDepth; iterationDepth++)
        {
            maxPlyReached = 0;

            i32 iterationScore = iterationDepth >= aspMinDepth() 
                                 ? aspiration(iterationDepth, score)
                                 : search(iterationDepth, 0, -INF, INF, DOUBLE_EXTENSIONS_MAX, false);
                                 
            if (stopSearch()) break;

            score = iterationScore;

            if (this != mainThread) continue;

            // Print uci info

            std::cout << "info"
                      << " depth "    << iterationDepth
                      << " seldepth " << (int)maxPlyReached;

            if (abs(score) < MIN_MATE_SCORE)
                std::cout << " score cp " << score;
            else
            {
                i32 pliesToMate = INF - abs(score);
                i32 movesTillMate = round(pliesToMate / 2.0);
                std::cout << " score mate " << (score > 0 ? movesTillMate : -movesTillMate);
            }

            u64 nodes = totalNodes();
            u64 msElapsed = millisecondsElapsed();

            std::cout << " nodes " << nodes
                      << " nps "   << nodes * 1000 / std::max(msElapsed, (u64)1)
                      << " time "  << msElapsed
                      << " pv "    << pliesData[0].pvString()
                      << std::endl;

            // Check soft limits
            if (nodes >= this->softNodes
            || millisecondsElapsed() >= (iterationDepth >= nodesTmMinDepth()
                                         ? softMilliseconds * nodesTmMultiplier()
                                           * (nodesTmBase() - bestMoveNodesFraction()) 
                                         : softMilliseconds))
                break;
        }

        // Signal secondary threads to stop
        if (this == mainThread) searchStopped = true;

        return score;
    }

    private:

    inline bool stopSearch()
    {
        // Only check time in main thread

        if (searchStopped) return true;

        if (this != mainThread) return false;

        if (bestMoveRoot() == MOVE_NONE) return false;

        if (hardNodes != I64_MAX && totalNodes() >= hardNodes) 
            return searchStopped = true;

        // Check time every N nodes
        return searchStopped = (nodes % 1024 == 0 && millisecondsElapsed() >= hardMilliseconds);
    }

    inline double bestMoveNodesFraction() {
        return (double)movesNodes[bestMoveRoot().encoded()] / std::max((double)nodes, 1.0);
    }

    inline void makeMove(Move move)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after the move
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) 
        {
            auto ttEntryIdx = TTEntryIndex(board.roughHashAfter(move), tt->size());
            __builtin_prefetch(&(*tt)[ttEntryIdx]);
        }

        board.makeMove(move);
        nodes++;

        plyDataPtr++;
        plyDataPtr->pvLength = 0;
        plyDataPtr->eval = INF;

        if (move != MOVE_NONE) {
            accumulatorPtr++;
            accumulatorPtr->updated = false;
        }
    }

    inline void undoMove()
    {
        if (board.lastMove() != MOVE_NONE)
            accumulatorPtr--;

        board.undoMove();
        plyDataPtr--;
    }

    inline i32 updateAccumulatorAndEval() 
    {
        assert(accumulatorPtr == &accumulators[0] ? accumulatorPtr->updated : (accumulatorPtr - 1)->updated);

        if (!accumulatorPtr->updated) 
            accumulatorPtr->update(accumulatorPtr - 1, board);

        if (board.inCheck())
            plyDataPtr->eval = INF;
        else if (plyDataPtr->eval == INF) {
            plyDataPtr->eval = evaluate(accumulatorPtr, board, true);

            // Adjust eval with correction history
            int stm = (int)board.sideToMove();
            plyDataPtr->eval += correctionHistory[stm][board.pawnHash() % 16384] / corrHistScale();

            plyDataPtr->eval = std::clamp(plyDataPtr->eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
        }

        return plyDataPtr->eval;
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
            score = search(depth, 0, alpha, beta, DOUBLE_EXTENSIONS_MAX, false);

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

    inline i32 search(i32 depth, i32 ply, i32 alpha, i32 beta, 
                      u8 doubleExtsLeft, bool cutNode, bool singular = false)
    { 
        assert(ply >= 0 && ply <= maxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta >= -INF && beta <= INF);
        assert(alpha < beta);

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (stopSearch()) return 0;

        if (ply > maxPlyReached) maxPlyReached = ply; // update seldepth

        if (depth > maxDepth) depth = maxDepth;

        bool pvNode = beta > alpha + 1;
        if (pvNode) assert(!cutNode);

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(board.zobristHash(), tt->size());
        TTEntry ttEntry = (*tt)[ttEntryIdx];
        bool ttHit = board.zobristHash() == ttEntry.zobristHash;

        // TT cutoff
        if (ttHit && !pvNode && !singular
        && ttEntry.depth >= depth 
        && (ttEntry.getBound() == Bound::EXACT
        || (ttEntry.getBound() == Bound::LOWER && ttEntry.score >= beta) 
        || (ttEntry.getBound() == Bound::UPPER && ttEntry.score <= alpha)))
            return ttEntry.adjustedScore(ply);

        if (ply >= maxDepth && board.inCheck()) return 0;

        i32 eval = updateAccumulatorAndEval();

        if (ply >= maxDepth) return eval;

        Color stm = board.sideToMove();
        (plyDataPtr + 1)->killer = MOVE_NONE;

        // If in check 2 plies ago, then pliesData[ply-2].eval is INF, and improving is false
        bool improving = ply > 1 && !board.inCheck() && eval > (plyDataPtr - 2)->eval;

        if (!pvNode && !singular && !board.inCheck())
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
            && board.lastMove() != MOVE_NONE 
            && eval >= beta
            && !(ttHit && ttEntry.getBound() == Bound::UPPER && ttEntry.score < beta)
            && board.hasNonPawnMaterial(stm))
            {
                makeMove(MOVE_NONE);

                i32 score = 0;

                if (!board.isDraw(ply)) {
                    i32 nmpDepth = depth - nmpBaseReduction() - depth / nmpReductionDivisor()
                                   - std::min((eval-beta) / nmpEvalBetaDivisor(), nmpEvalBetaMax());

                    score = -search(nmpDepth, ply + 1, -beta, -alpha, doubleExtsLeft, !cutNode);
                }

                undoMove();

                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }

            if (stopSearch()) return 0;
        }

        if (!ttHit) ttEntry.move = MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth() && ttEntry.move == MOVE_NONE && (pvNode || cutNode))
            depth--;

        // genenerate and score all moves except underpromotions
        if (!singular) genAndScoreMoves(false, ttEntry.move);

        u64 pinned = board.pinned();
        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        bool isBestMoveQuiet = false;
        Bound bound = Bound::UPPER;

        // Fail low quiets at beginning of array, fail low noisy moves at the end
        std::array<HistoryEntry*, 256> failLowsHistoryEntry;
        int failLowQuiets = 0, failLowNoisies = 0;

        for (int i = 0; i < plyDataPtr->moves.size(); i++)
        {
            auto [move, moveScore] = plyDataPtr->nextMove(i);

            // Don't search TT move in singular search
            if (move == ttEntry.move && singular) continue;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            legalMovesSeen++;

            PieceType captured = board.captured(move);
            bool isQuiet = captured == PieceType::NONE && move.promotion() == PieceType::NONE;

            PieceType pt = move.pieceType();
            HistoryEntry *historyEntry = &historyTable[(int)stm][(int)pt][move.to()];

            if (ply > 0 && bestScore > -MIN_MATE_SCORE && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves() + pvNode + board.inCheck()
                                      + depth * depth * lmpMultiplier() / (improving ? 1 : 2))
                    break;

                i32 lmrDepth = std::max(0, depth - LMR_TABLE[depth][legalMovesSeen] - !improving);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth() 
                && !board.inCheck()
                && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase() + lmrDepth * fpMultiplier())
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth()) 
                {
                    i32 threshold = isQuiet ? lmrDepth * seeQuietThreshold() 
                                              - moveScore / seeQuietHistoryDiv()
                                            : depth * depth * seeNoisyThreshold() 
                                              - historyEntry->noisyHistory(captured) / seeNoisyHistoryDiv();

                    if (!SEE(board, move, std::min(threshold, -1))) continue;
                }
            }

            i32 extension = 0;

            // SE (Singular extensions)
            if (move == ttEntry.move
            && ply > 0
            && depth >= singularMinDepth()
            && abs(ttEntry.score) < MIN_MATE_SCORE
            && (i32)ttEntry.depth >= depth - singularDepthMargin()
            && ttEntry.getBound() != Bound::UPPER)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                i32 singularBeta = std::max(-INF, (i32)ttEntry.score - i32(depth * singularBetaMultiplier()));

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           doubleExtsLeft, cutNode, true);

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
            }

            u64 nodesBefore = nodes;
            makeMove(move);

            // Check extension if no singular extensions
            if (extension == 0) extension = board.inCheck();

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;

            if (board.isDraw(ply)) goto moveSearched;

            if (legalMovesSeen == 1) {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft, false);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && legalMovesSeen >= lmrMinMoves() && moveScore < COUNTERMOVE_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesSeen];
                lmr -= pvNode;          // reduce pv nodes less
                lmr -= board.inCheck(); // reduce less if move gives check
                lmr -= improving;       // reduce less if were improving
                lmr += 2 * cutNode;     // reduce more if we expect to fail high
                
                // reduce moves with good history less and vice versa
                lmr -= round(isQuiet ? (float)moveScore / (float)lmrQuietHistoryDiv()
                                     : (float)historyEntry->noisyHistory(captured) / (float)lmrNoisyHistoryDiv());

                lmr = std::clamp(lmr, 0, depth - 2); // dont extend or reduce into qsearch
            }

            score = -search(depth - 1 - lmr + extension, ply + 1, -alpha-1, -alpha, doubleExtsLeft, true);

            if (score > alpha && lmr > 0)
                score = -search(depth - 1 + extension, ply + 1, -alpha-1, -alpha, doubleExtsLeft, !cutNode); 

            if (score > alpha && pvNode)
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft, false);

            moveSearched:

            undoMove();

            if (stopSearch()) return 0;

            if (ply == 0) movesNodes[move.encoded()] += nodes - nodesBefore;

            if (score > bestScore) bestScore = score;

            if (score <= alpha) // Fail low
            {
                int index = isQuiet ? failLowQuiets++ : 256 - ++failLowNoisies;
                failLowsHistoryEntry[index] = historyEntry;
                continue;
            }

            alpha = score;
            bestMove = move;
            isBestMoveQuiet = isQuiet;
            bound = Bound::EXACT;
            
            // Update PV
            if (pvNode) {
                plyDataPtr->pvLength = 1 + (plyDataPtr + 1)->pvLength;
                plyDataPtr->pvLine[0] = move;

                // Copy child's PV
                for (int idx = 0; idx < (plyDataPtr + 1)->pvLength; idx++)
                    plyDataPtr->pvLine[idx + 1] = (plyDataPtr + 1)->pvLine[idx];            
            }

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            i32 historyBonus = std::clamp(depth * historyBonusMultiplier() - historyBonusOffset(), 0, historyBonusMax());
            i32 historyMalus = -std::clamp(depth * historyMalusMultiplier() - historyMalusOffset(), 0, historyMalusMax());

            if (isQuiet) {
                // This fail high quiet is now a killer move
                plyDataPtr->killer = move; 

                // This fail high quiet is now a countermove
                if (board.lastMove() != MOVE_NONE) {
                    int nstm = (int)board.oppSide();
                    countermoves[nstm][board.lastMove().encoded()] = move;
                }

                // Increase history of this fail high quiet move
                historyEntry->updateQuietHistory(historyBonus, board);

                // History malus: decrease history of fail low quiets
                for (int j = 0; j < failLowQuiets; j++)
                    failLowsHistoryEntry[j]->updateQuietHistory(historyMalus, board);
            }
            else 
                // Increase history of this fail high noisy move
                historyEntry->updateNoisyHistory(historyBonus, captured);

            // History malus: decrease history of fail low noisy moves
            for (int idx = 255, penalized = 0; penalized < failLowNoisies; idx--, penalized++)
                failLowsHistoryEntry[idx]->updateNoisyHistory(historyMalus, captured);

            break; // Fail high / beta cutoff
        }

        if (legalMovesSeen == 0) 
            // checkmate or stalemate
            return board.inCheck() ? -INF + ply : 0;

        if (!singular)
        {
            (*tt)[ttEntryIdx].update(board.zobristHash(), depth, ply, bestScore, bestMove, bound);

            // Update correction history
            if (!board.inCheck()
            && (bestMove == MOVE_NONE || isBestMoveQuiet)
            && !(bound == Bound::LOWER && bestScore <= eval)
            && !(bound == Bound::UPPER && bestScore >= eval))
            {
                i32* corrHist = &correctionHistory[(int)stm][board.pawnHash() % 16384];
                i32 newWeight = std::min(depth + 1, corrHistNewWeightMin());

                *corrHist *= corrHistScale() - newWeight;
                *corrHist += (bestScore - eval) * corrHistScale() * newWeight;
                *corrHist = std::clamp(*corrHist / corrHistScale(), -corrHistMax(), corrHistMax());
            }
        }

        return bestScore;
    }

    // Quiescence search
    inline i32 qSearch(i32 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= maxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta >= -INF && beta <= INF);
        assert(alpha < beta);

        if (stopSearch()) return 0;

        if (ply > maxPlyReached) maxPlyReached = ply; // update seldepth

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(board.zobristHash(), tt->size());
        TTEntry *ttEntry = &(*tt)[ttEntryIdx];
        bool ttHit = board.zobristHash() == ttEntry->zobristHash;

        // TT cutoff
        if (ttHit
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        if (ply >= maxDepth && board.inCheck()) return 0;

        i32 eval = updateAccumulatorAndEval();

        if (!board.inCheck()) 
        {
            if (ply >= maxDepth || eval >= beta) return eval; 

            if (eval > alpha) alpha = eval;
        }

        (plyDataPtr + 1)->killer = MOVE_NONE;

        // if in check, generate all moves, else only noisy moves
        // never generate underpromotions
        genAndScoreMoves(!board.inCheck(), ttHit ? ttEntry->move : MOVE_NONE);

        u64 pinned = board.pinned();
        int legalMovesPlayed = 0;
        i32 bestScore = board.inCheck() ? -INF : eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        for (int i = 0; i < plyDataPtr->moves.size(); i++)
        {
            auto [move, moveScore] = plyDataPtr->nextMove(i);

            // SEE pruning (skip bad noisy moves)
            if (!board.inCheck() && moveScore < 0) break;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            makeMove(move);
            legalMovesPlayed++;

            i32 score = board.isDraw(ply) ? 0 : -qSearch(ply + 1, -beta, -alpha);

            undoMove();

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

        if (legalMovesPlayed == 0 && board.inCheck()) 
            // checkmate
            return -INF + ply; 

        ttEntry->update(board.zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    constexpr static i32 
        GOOD_QUEEN_PROMO_SCORE = 1'600'000'000,
        GOOD_NOISY_SCORE       = 1'500'000'000,
        KILLER_SCORE           = 1'000'000'000,
        COUNTERMOVE_SCORE      = 500'000'000;

    inline void genAndScoreMoves(bool noisiesOnly, Move ttMove)
    {
        // never generate underpromotions in search
        board.pseudolegalMoves(plyDataPtr->moves, noisiesOnly, false);

        int stm = (int)board.sideToMove();
        int nstm = (int)board.oppSide();

        Move countermove = countermoves[nstm][board.lastMove().encoded()];

        if (board.lastMove() == MOVE_NONE) 
            assert(countermove == MOVE_NONE);

        for (int i = 0; i < plyDataPtr->moves.size(); i++)
        {
            Move move = plyDataPtr->moves[i];
            i32 *moveScore = &(plyDataPtr->movesScores[i]);

            if (move == ttMove) {
                *moveScore = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();
            int pt = (int)move.pieceType();
            HistoryEntry *historyEntry = &historyTable[stm][pt][move.to()];

            if (captured != PieceType::NONE)
            {
                *moveScore = SEE(board, move) 
                             ? (promotion == PieceType::QUEEN
                                ? GOOD_QUEEN_PROMO_SCORE 
                                : GOOD_NOISY_SCORE)
                             : -GOOD_NOISY_SCORE;

                *moveScore += ((i32)captured + 1) * 1'000'000; // MVV (most valuable victim)
                *moveScore += historyEntry->noisyHistory(captured);
            }
            else if (promotion == PieceType::QUEEN)
            {
                *moveScore = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE : -GOOD_NOISY_SCORE;
                *moveScore += historyEntry->noisyHistory(captured);
            }
            else if (move == plyDataPtr->killer)
                *moveScore = KILLER_SCORE;
            else if (move == countermove)
                *moveScore = COUNTERMOVE_SCORE;
            else
                *moveScore = historyEntry->quietHistory(board);
            
        }
    }

}; // class SearchThread

std::vector<SearchThread> searchThreads; // the main thread is created in main()

inline u64 totalNodes() {
    u64 nodes = 0;

    for (SearchThread &searchThread : searchThreads)
        nodes += searchThread.getNodes();

    return nodes;
}