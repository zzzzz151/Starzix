#pragma once

// clang-format off

const uint8_t MAX_DEPTH = 100;

const int ASPIRATION_MIN_DEPTH = 6,
          ASPIRATION_INITIAL_DELTA = 25;
const double ASPIRATION_DELTA_MULTIPLIER = 1.5;

const int IIR_MIN_DEPTH = 4;

const int RFP_MAX_DEPTH = 8,
          RFP_DEPTH_MULTIPLIER = 75;

// Alpha pruning
const int AP_MAX_DEPTH = 4,
          AP_EVAL_MARGIN = 1750;

const int RAZORING_MAX_DEPTH = 6,
          RAZORING_DEPTH_MULTIPLIER = 252;

const int NMP_MIN_DEPTH = 3,
          NMP_BASE_REDUCTION = 3,
          NMP_REDUCTION_DIVISOR = 3;

const int LMP_MAX_DEPTH = 8,
          LMP_MIN_MOVES_BASE = 3;
const double LMP_DEPTH_MULTIPLIER = 0.75;

const int FP_MAX_DEPTH = 7,
          FP_BASE = 120,
          FP_MULTIPLIER = 65;

const int SEE_PRUNING_MAX_DEPTH = 9,
          SEE_PRUNING_NOISY_THRESHOLD = -20,
          SEE_PRUNING_QUIET_THRESHOLD = -65;

const int SINGULAR_MIN_DEPTH = 8,
          SINGULAR_DEPTH_MARGIN = 3,
          SINGULAR_BETA_DEPTH_MULTIPLIER = 2,
          SINGULAR_BETA_MARGIN = 24,
          SINGULAR_MAX_DOUBLE_EXTENSIONS = 5;

const int LMR_MIN_DEPTH = 3;
const double LMR_BASE = 1,
             LMR_MULTIPLIER = 0.5;
const int LMR_HISTORY_DIVISOR = 8192;

const int32_t HISTORY_MIN_BONUS = 1570,
              HISTORY_BONUS_MULTIPLIER = 370,
              HISTORY_MAX = 16384;

const int16_t POS_INFINITY = 32000, 
              NEG_INFINITY = -POS_INFINITY, 
              MIN_MATE_SCORE = POS_INFINITY - MAX_DEPTH*2;

#include "time_management.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "nnue.hpp"

Board board;
uint64_t nodes;
int maxPlyReached;
uint64_t movesNodes[1ULL << 16];        // [move]
Move pvLines[MAX_DEPTH+1][MAX_DEPTH+1]; // [ply][ply]
int pvLengths[MAX_DEPTH+1];             // [pvLineIndex]
int lmrTable[MAX_DEPTH+1][256];         // [depth][moveIndex]
int doubleExtensions[MAX_DEPTH+1];      // [ply]
Move killerMoves[MAX_DEPTH];            // [ply]
Move countermoves[2][1ULL << 16];       // [color][move]
HistoryEntry historyTable[2][6][64];    // [color][pieceType][targetSquare]

#include "move_scoring.hpp"

namespace uci
{
    inline void info(int depth, int16_t score); 
}

inline void initSearch()
{
    // init lmrTable
    for (int depth = 0; depth < MAX_DEPTH+1; depth++)
        for (int move = 0; move < 256; move++)
            lmrTable[depth][move] = depth == 0 || move == 0 ? 0 : round(LMR_BASE + ln(depth) * ln(move) * LMR_MULTIPLIER);
}

inline int16_t evaluate()
{
    return clamp(nnue::evaluate(board.sideToMove()), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

inline int16_t qSearch(int plyFromRoot, int16_t alpha, int16_t beta)
{
    // Quiescence search: search captures until a 'quiet' position is reached

    // Update seldepth
    if (plyFromRoot > maxPlyReached) maxPlyReached = plyFromRoot;

    if (board.isDraw()) return 0;

    if (plyFromRoot >= MAX_DEPTH) 
        return board.inCheck() ? 0 : evaluate();

    int16_t eval = NEG_INFINITY; // eval in check
    if (!board.inCheck())
    {
        eval = evaluate();
        if (eval >= beta) return eval;
        if (eval > alpha) alpha = eval;
    }

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), 0, plyFromRoot, alpha, beta);
    if (shouldCutoff) return ttEntry->adjustedScore(plyFromRoot);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash ? ttEntry->bestMove : NULL_MOVE;

    // if in check, generate and score all moves, else only captures
    MovesList moves = board.pseudolegalMoves(!board.inCheck()); 
    array<int32_t, 256> movesScores = scoreMoves(moves, ttMove, killerMoves[plyFromRoot], board.inCheck());
    
    int legalMovesPlayed = 0;
    int16_t bestScore = eval;
    Move bestMove = NULL_MOVE;
    int16_t originalAlpha = alpha;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // SEE pruning (skip bad captures)
        if (!board.inCheck() && !SEE(board, move)) continue;

        // skip illegal moves
        if (!board.makeMove(move)) continue; 

        nodes++;
        legalMovesPlayed++;

        int16_t score = -qSearch(plyFromRoot + 1, -beta, -alpha);
        board.undoMove();

        if (score <= bestScore) continue;
        bestScore = score;
        bestMove = move;

        if (bestScore >= beta) break;
        if (bestScore > alpha) alpha = bestScore;
    }

    if (board.inCheck() && legalMovesPlayed == 0) 
        // checkmate
        return NEG_INFINITY + plyFromRoot; 

    storeInTT(ttEntry, board.getZobristHash(), 0, bestScore, bestMove, plyFromRoot, originalAlpha, beta);    

    return bestScore;
}

inline int16_t PVS(int depth, int plyFromRoot, int16_t alpha, int16_t beta, bool cutNode, Move singularMove = NULL_MOVE, int16_t eval = 0)
{
    if (isHardTimeUp()) return 0;

    pvLengths[plyFromRoot] = 0; // Ensure fresh PV

    depth = clamp(depth, 0, (int)MAX_DEPTH);

    // Check extension
    if (plyFromRoot > 0 && depth < MAX_DEPTH) 
        depth += board.inCheck();

    // Drop into qsearch on terminal nodes
    if (depth <= 0) return qSearch(plyFromRoot, alpha, beta);

    // Update seldepth
    if (plyFromRoot > maxPlyReached) maxPlyReached = plyFromRoot;

    if (plyFromRoot > 0 && board.isDraw()) return 0;

    if (plyFromRoot >= MAX_DEPTH) 
        return board.inCheck() ? 0 : evaluate();

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), depth, plyFromRoot, alpha, beta);
    if (shouldCutoff && singularMove == NULL_MOVE) 
        return ttEntry->adjustedScore(plyFromRoot);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash && singularMove == NULL_MOVE
                  ? ttEntry->bestMove : NULL_MOVE;

    Color stm = board.sideToMove();
    bool pvNode = beta - alpha > 1 || plyFromRoot == 0;

    // We don't use eval in check because it's unreliable, so don't bother calculating it if in check
    // In singular search we already have the eval, passed in the eval arg
    if (!board.inCheck() && singularMove == NULL_MOVE)
        eval = evaluate();

    if (!pvNode && !board.inCheck() && singularMove == NULL_MOVE)
    {
        // RFP (Reverse futility pruning) / Static NMP
        if (depth <= RFP_MAX_DEPTH && eval >= beta + depth * RFP_DEPTH_MULTIPLIER)
            return eval;

        // AP (Alpha pruning)
        if (depth <= AP_MAX_DEPTH && eval + AP_EVAL_MARGIN <= alpha)
            return eval; 

        // Razoring
        if (depth <= RAZORING_MAX_DEPTH && alpha > eval + depth * RAZORING_DEPTH_MULTIPLIER) {
            int16_t score = qSearch(plyFromRoot, alpha, beta);
            if (score <= alpha) return score;
        }

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && board.getLastMove() != NULL_MOVE
        && board.hasNonPawnMaterial(stm) && eval >= beta)
        {
            board.makeNullMove();
            int nmpDepth = depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR - min((eval - beta)/200, 3);
            int16_t score = -PVS(nmpDepth, plyFromRoot + 1, -beta, -alpha, !cutNode);
            board.undoNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    bool trySingular = singularMove == NULL_MOVE && depth >= SINGULAR_MIN_DEPTH && plyFromRoot > 0
                       && ttMove != NULL_MOVE && abs(ttEntry->score) < MIN_MATE_SCORE
                       && ttEntry->depth >= depth - SINGULAR_DEPTH_MARGIN && ttEntry->getBound() != UPPER_BOUND;
                       
    // IIR (Internal iterative reduction)
    if (ttMove == NULL_MOVE && depth >= IIR_MIN_DEPTH && !board.inCheck())
        depth--;

    MovesList moves = board.pseudolegalMoves();
    array<int32_t, 256> movesScores = scoreMoves(moves, ttMove, killerMoves[plyFromRoot]);

    int legalMovesPlayed = 0;
    int16_t bestScore = NEG_INFINITY;
    Move bestMove = NULL_MOVE;
    int16_t originalAlpha = alpha;
    doubleExtensions[plyFromRoot + 1] = doubleExtensions[plyFromRoot];

    // Fail low quiets at beginning of array, fail low captures at the end
    HistoryEntry *pointersFailLowsHistoryEntry[256]; 
    int numFailLowQuiets = 0, numFailLowCaptures = 0;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // don't search singular move in singular search
        if (move == singularMove) continue;

        bool isCapture = board.isCapture(move);
        bool isQuietMove = !isCapture && move.promotionPieceType() == PieceType::NONE;
        bool historyMoveOrLosing = moveScore < COUNTERMOVE_SCORE;
        int lmr = lmrTable[depth][legalMovesPlayed + 1];

        // Moves loop pruning
        if (plyFromRoot > 0 && historyMoveOrLosing && bestScore > -MIN_MATE_SCORE)
        {
            // LMP (Late move pruning)
            if (depth <= LMP_MAX_DEPTH 
            && legalMovesPlayed >= LMP_MIN_MOVES_BASE + pvNode + board.inCheck() + depth * depth * LMP_DEPTH_MULTIPLIER)
                break;

            // FP (Futility pruning)
            if (depth <= FP_MAX_DEPTH && !board.inCheck() && alpha < MIN_MATE_SCORE 
            && eval + FP_BASE + max(depth - lmr, 0) * FP_MULTIPLIER <= alpha)
                break;

            // SEE pruning
            int threshold = isQuietMove ? depth * SEE_PRUNING_QUIET_THRESHOLD : depth * depth * SEE_PRUNING_NOISY_THRESHOLD;
            if (depth <= SEE_PRUNING_MAX_DEPTH && !SEE(board, move, threshold))
                continue;
        }

        // skip illegal moves
        if (!board.makeMove(move)) continue; 

        uint64_t prevNodes = nodes;
        nodes++;
        legalMovesPlayed++;

        // Extensions
        int extension = 0;
        // SE (Singular extensions)
        if (trySingular && move == ttEntry->bestMove)
        {
            // Singular search: before searching any move, search this node at a shallower depth with TT move excluded
            board.undoMove(); // undo TT move we just made
            int16_t singularBeta = ttEntry->score - depth * SINGULAR_BETA_DEPTH_MULTIPLIER;
            int16_t singularScore = PVS((depth - 1) / 2, plyFromRoot, singularBeta - 1, singularBeta, cutNode, move, eval);
            board.makeMove(move, false); // second arg = false => don't check legality (we already verified it's a legal move)

            if (!pvNode && singularScore < singularBeta && singularScore < singularBeta - SINGULAR_BETA_MARGIN 
            && doubleExtensions[plyFromRoot + 1] < SINGULAR_MAX_DOUBLE_EXTENSIONS)
            {
                // Double extension
                // singularScore is way lower than TT score
                // TT move is probably MUCH better than all others, so extend its search by 2 plies
                extension = 2;
                doubleExtensions[plyFromRoot + 1]++; // Keep track of these double extensions so search doesn't explode
            }
            else if (singularScore < singularBeta)
                // Normal singular extension
                // TT move is probably better than all others, so extend its search by 1 ply
                extension = 1;
            // Negative extensions
            else if (ttEntry->score >= beta)
                // some other move is probably better than TT move, so reduce TT move search by 2 plies
                extension = -2;
            else if (cutNode)
                extension = -1;
        }
        
        // In PVS (Principal Variation Search), the highest ranked move (TT move if one exists) is searched normally, with a full window
        int16_t score = 0;
        if (legalMovesPlayed == 1)
        {
            score = -PVS(depth - 1 + extension, plyFromRoot + 1, -beta, -alpha, false);
            goto searchingDone;
        }
        
        // LMR (Late move reductions)
        if (depth >= LMR_MIN_DEPTH && historyMoveOrLosing)
        {
            //lmr -= board.inCheck(); // reduce checks less
            lmr -= pvNode; // reduce pv nodes less

            // reduce quiets with good history less and vice versa
            if (isQuietMove) 
                lmr -= round((moveScore - HISTORY_MOVE_BASE_SCORE) / (double)LMR_HISTORY_DIVISOR);

            // if lmr is negative, we would have an extension instead of a reduction
            if (lmr < 0) lmr = 0;
        }
        else
            lmr = 0;

        // In PVS, the other moves (not TT move) are searched with a null/zero window
        // and researched with a full window if they're better than expected (score > alpha)
        score = -PVS(depth - 1 - lmr, plyFromRoot + 1, -alpha - 1, -alpha, true);
        if (score > alpha && (score < beta || lmr > 0))
            score = -PVS(depth - 1, plyFromRoot + 1, -beta, -alpha, score < beta ? false : !cutNode);

        searchingDone:

        board.undoMove();
        if (isHardTimeUp()) return 0;

        if (plyFromRoot == 0)
            movesNodes[move.getMove()] += nodes - prevNodes;

        if (score > bestScore) bestScore = score;

        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();

        if (score <= alpha) // Fail low
        {
            // Fail low quiets at beginning of array, fail low captures at the end
            if (isQuietMove)
                pointersFailLowsHistoryEntry[numFailLowQuiets++] = &(historyTable[stm][pieceType][targetSquare]);
            else if (isCapture)
                pointersFailLowsHistoryEntry[256 - ++numFailLowCaptures] = &(historyTable[stm][pieceType][targetSquare]);

            continue;
        }

        alpha = score;
        bestMove = move;

        if (pvNode)
        {
            // Update pv line
            int subPvLineLength = pvLengths[plyFromRoot + 1];
            pvLengths[plyFromRoot] = 1 + subPvLineLength;
            pvLines[plyFromRoot][0] = move;
            memcpy(&(pvLines[plyFromRoot][1]), pvLines[plyFromRoot + 1], subPvLineLength * sizeof(Move)); // dst, src, size
        }

        if (score < beta) continue;

        // Fail high / Beta cutoff

        int32_t historyBonus = min(HISTORY_MIN_BONUS, HISTORY_BONUS_MULTIPLIER * (depth - 1));

        if (isQuietMove)
        {
            // This quiet move is a killer move and a countermove
            killerMoves[plyFromRoot] = move;
            if (board.getLastMove() != NULL_MOVE)
                countermoves[stm][board.getLastMove().getMove()] = move;

            // Increase this quiet's history
            historyTable[stm][pieceType][targetSquare].updateQuietHistory(board, historyBonus);

            // Penalize/decrease history of quiets that failed low
            for (int i = 0; i < numFailLowQuiets; i++)
                pointersFailLowsHistoryEntry[i]->updateQuietHistory(board, -historyBonus); 
        }
        else if (isCapture)
        {
            // Increase this capture's history
            historyTable[stm][pieceType][targetSquare].updateCaptureHistory(board, historyBonus);

            // Penalize/decrease history of captures that failed low
            for (int i = 255, j = 0; j < numFailLowCaptures; i--, j++)
                pointersFailLowsHistoryEntry[i]->updateCaptureHistory(board, -historyBonus);
        }

        break; // Fail high / Beta cutoff
    }

    if (legalMovesPlayed == 0) 
        // checkmate or stalemate
        return board.inCheck() ? NEG_INFINITY + plyFromRoot : 0;

    if (singularMove == NULL_MOVE)
        storeInTT(ttEntry, board.getZobristHash(), depth, bestScore, bestMove, plyFromRoot, originalAlpha, beta);    

    return bestScore;
}

inline int16_t aspiration(int maxDepth, int16_t score)
{
    // Aspiration Windows
    // Search with a small window, adjusting it and researching until the score is inside the window

    int16_t delta = ASPIRATION_INITIAL_DELTA;
    int16_t alpha = max(NEG_INFINITY, score - delta);
    int16_t beta = min(POS_INFINITY, score + delta);
    int depth = maxDepth;

    while (true)
    {
        score = PVS(depth, 0, alpha, beta, false);

        if (isHardTimeUp()) return 0;

        if (score >= beta)
        {
            beta = min(beta + delta, POS_INFINITY);
            depth--;
        }
        else if (score <= alpha)
        {
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, NEG_INFINITY);
            depth = maxDepth;
        }
        else
            break;

        delta *= ASPIRATION_DELTA_MULTIPLIER;
    }

    return score;
}

inline Move iterativeDeepening()
{
    // clear stuff
    nodes = 0;
    memset(movesNodes, 0, sizeof(movesNodes));
    memset(pvLines, 0, sizeof(pvLines));
    memset(pvLengths, 0, sizeof(pvLengths));
    memset(doubleExtensions, 0, sizeof(doubleExtensions));

    // ID (Iterative Deepening)
    int16_t score = 0;
    for (int iterationDepth = 1; iterationDepth <= MAX_DEPTH; iterationDepth++)
    {
        maxPlyReached = -1;

        score = iterationDepth >= ASPIRATION_MIN_DEPTH 
                ? aspiration(iterationDepth, score) 
                : PVS(iterationDepth, 0, NEG_INFINITY, POS_INFINITY, false);

        if (isHardTimeUp()) break;

        uci::info(iterationDepth, score);

        double bestMoveNodesFraction = (double)movesNodes[pvLines[0][0].getMove()] / (double)nodes;
        if (isSoftTimeUp(bestMoveNodesFraction)) break;
    }

    if (ttAge < 63) ttAge++;

    return pvLines[0][0]; // return best move
}

