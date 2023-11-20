#pragma once

// clang-format off

#include "time_management.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "nnue.hpp"

const u8 MAX_DEPTH = 100;

// Move ordering
const i32 TT_MOVE_SCORE           = MAX_INT32,
          GOOD_CAPTURE_BASE_SCORE = 1'500'000'000,
          PROMOTION_BASE_SCORE    = 1'000'000'000,
          KILLER_SCORE            = 500'000'000,
          COUNTERMOVE_SCORE       = 250'000'000,
          HISTORY_MOVE_BASE_SCORE = 0,
          BAD_CAPTURE_BASE_SCORE  = -500'000'000;

// Most valuable victim    P    N    B    R    Q
const i32 MVV_VALUES[5] = {100, 300, 320, 500, 900};

const int ASPIRATION_MIN_DEPTH = 6,
          ASPIRATION_INITIAL_DELTA = 15;
const double ASPIRATION_DELTA_MULTIPLIER = 1.2;

const int IIR_MIN_DEPTH = 4;

const int RFP_MAX_DEPTH = 8,
          RFP_DEPTH_MULTIPLIER = 75;

// Alpha pruning
const int AP_MAX_DEPTH = 4,
          AP_MARGIN = 1750;

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

const i32 HISTORY_MIN_BONUS = 1570,
          HISTORY_BONUS_MULTIPLIER = 370,
          HISTORY_MAX = 16384;

Board board;
u64 nodes;
int maxPlyReached;
u64 movesNodes[1ULL << 16];             // [moveEncoded]
Move pvLines[MAX_DEPTH+1][MAX_DEPTH+1]; // [ply][ply]
int pvLengths[MAX_DEPTH+1];             // [pvLineIndex]
int lmrTable[MAX_DEPTH+1][256];         // [depth][moveIndex]
Move killerMoves[MAX_DEPTH];            // [ply]
Move countermoves[2][1ULL << 16];       // [color][moveEncoded]
HistoryEntry historyTable[2][6][64];    // [color][pieceType][targetSquare]

namespace uci { 
    inline void info(int depth, i16 score); 
}

inline i16 aspiration(int maxDepth, i16 score);

inline i16 search(int depth, int ply, i16 alpha, i16 beta, bool cutNode, 
                  int doubleExtensionsLeft, bool singular = false, i16 eval = 0);

inline i16 qSearch(int ply, i16 alpha, i16 beta);

inline array<i32, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove, bool doSEE = true);

inline void initSearch()
{
    // init lmrTable
    for (int depth = 0; depth < MAX_DEPTH+1; depth++)
        for (int move = 0; move < 256; move++)
            lmrTable[depth][move] = depth == 0 || move == 0 
                                    ? 0 : round(LMR_BASE + ln(depth) * ln(move) * LMR_MULTIPLIER);
}

inline Move iterativeDeepening()
{
    // clear stuff
    nodes = 0;
    memset(movesNodes, 0, sizeof(movesNodes));
    memset(pvLines, 0, sizeof(pvLines));
    memset(pvLengths, 0, sizeof(pvLengths));

    // ID (Iterative Deepening)
    i16 score = 0;
    for (int iterationDepth = 1; iterationDepth <= MAX_DEPTH; iterationDepth++)
    {
        maxPlyReached = -1;

        score = iterationDepth >= ASPIRATION_MIN_DEPTH 
                ? aspiration(iterationDepth, score) 
                : search(iterationDepth, 0, NEG_INFINITY, POS_INFINITY, false, SINGULAR_MAX_DOUBLE_EXTENSIONS);

        if (isHardTimeUp(nodes)) break;

        uci::info(iterationDepth, score);

        double bestMoveNodesFraction = (double)movesNodes[pvLines[0][0].getMoveEncoded()] / (double)nodes;
        if (isSoftTimeUp(bestMoveNodesFraction)) break;
    }

    if (ttAge < 63) ttAge++;

    return pvLines[0][0]; // return best move
}

inline i16 aspiration(int maxDepth, i16 score)
{
    // Aspiration Windows
    // Search with a small window, adjusting it and researching until the score is inside the window

    i16 delta = ASPIRATION_INITIAL_DELTA;
    i16 alpha = max(NEG_INFINITY, score - delta);
    i16 beta = min(POS_INFINITY, score + delta);
    int depth = maxDepth;

    while (true)
    {
        score = search(depth, 0, alpha, beta, false, SINGULAR_MAX_DOUBLE_EXTENSIONS);

        if (isHardTimeUp(nodes)) return 0;

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

inline i16 evaluate() {
    return clamp(nnue::evaluate(board.sideToMove()), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

inline i16 search(int depth, int ply, i16 alpha, i16 beta, bool cutNode, 
                  int doubleExtensionsLeft, bool singular, i16 eval)
{
    if (isHardTimeUp(nodes)) return 0;

    pvLengths[ply] = 0; // Ensure fresh PV

    // Drop into qsearch on terminal nodes
    if (depth <= 0) return qSearch(ply, alpha, beta);

    // Update seldepth
    if (ply > maxPlyReached) maxPlyReached = ply;

    if (ply > 0 && board.isDraw()) return 0;

    if (ply >= MAX_DEPTH) 
        return board.inCheck() ? 0 : evaluate();

    if (depth > MAX_DEPTH) depth = MAX_DEPTH;

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), depth, ply, alpha, beta);
    if (shouldCutoff && !singular) 
        return ttEntry->adjustedScore(ply);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash && !singular
                  ? ttEntry->bestMove : MOVE_NONE;

    bool pvNode = beta - alpha > 1 || ply == 0;
    if (cutNode) assert(!pvNode); // cutNode implies !pvNode

    // We don't use eval in check because it's unreliable, so don't bother calculating it if in check
    // In singular search we already have the eval, passed in the eval arg
    if (!board.inCheck() && !singular)
        eval = evaluate();

    if (!pvNode && !board.inCheck() && !singular)
    {
        // RFP (Reverse futility pruning) / Static NMP
        if (depth <= RFP_MAX_DEPTH && eval >= beta + depth * RFP_DEPTH_MULTIPLIER)
            return eval;

        // AP (Alpha pruning)
        if (depth <= AP_MAX_DEPTH && eval + AP_MARGIN <= alpha)
            return eval; 

        // Razoring
        if (depth <= RAZORING_MAX_DEPTH && alpha > eval + depth * RAZORING_DEPTH_MULTIPLIER) {
            i16 score = qSearch(ply, alpha, beta);
            if (score <= alpha) return score;
        }

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && board.getLastMove() != MOVE_NONE
        && board.hasNonPawnMaterial(board.sideToMove()) && eval >= beta)
        {
            board.makeNullMove();
            int nmpDepth = depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR - min((eval - beta)/200, 3);
            i16 score = -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtensionsLeft);
            board.undoNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    bool trySingular = !singular && depth >= SINGULAR_MIN_DEPTH && ply > 0
                       && ttMove != MOVE_NONE && abs(ttEntry->score) < MIN_MATE_SCORE
                       && ttEntry->depth >= depth - SINGULAR_DEPTH_MARGIN && ttEntry->getBound() != UPPER_BOUND;
                       
    // IIR (Internal iterative reduction)
    if (ttMove == MOVE_NONE && depth >= IIR_MIN_DEPTH && !board.inCheck())
        depth--;

    MovesList moves = board.pseudolegalMoves();
    auto movesScores = scoreMoves(moves, ttMove, killerMoves[ply]);

    int stm = (int)board.sideToMove();
    int legalMovesPlayed = 0;
    i16 bestScore = NEG_INFINITY;
    Move bestMove = MOVE_NONE;
    i16 originalAlpha = alpha;

    // Fail low quiets at beginning of array, fail low captures at the end
    HistoryEntry *failLowsHistoryEntry[256]; 
    int numFailLowQuiets = 0, numFailLowCaptures = 0;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // Don't search TT move in singular search
        if (singular && move == ttEntry->bestMove) continue;

        bool isCapture = board.isCapture(move);
        bool isQuietMove = !isCapture && move.promotionPieceType() == PieceType::NONE;
        bool historyMoveOrLosing = moveScore < COUNTERMOVE_SCORE;
        int lmr = lmrTable[depth][legalMovesPlayed + 1];

        // Moves loop pruning
        if (ply > 0 && historyMoveOrLosing && bestScore > -MIN_MATE_SCORE)
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

        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();

        // skip illegal moves
        if (!board.makeMove(move)) continue; 

        u64 prevNodes = nodes;
        nodes++;
        legalMovesPlayed++;

        int extension = 0;
        if (ply == 0) goto skipExtensions;

        // Extensions
        // SE (Singular extensions)
        if (trySingular && move == ttEntry->bestMove)
        {
            // Singular search: before searching any move, search this node at a shallower depth with TT move excluded

            board.undoMove(); // undo TT move we just made

            i16 singularBeta = ttEntry->score - depth * SINGULAR_BETA_DEPTH_MULTIPLIER;
            i16 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                       cutNode, doubleExtensionsLeft, true, eval);

            board.makeMove(move, false); // second arg = false => don't check legality (we already verified it's a legal move)

            // Double extension
            if (!pvNode && doubleExtensionsLeft > 0 && singularScore < singularBeta - SINGULAR_BETA_MARGIN)
            {
                // singularScore is way lower than TT score
                // TT move is probably MUCH better than all others, so extend its search by 2 plies
                extension = 2;
                doubleExtensionsLeft--;
            }
            // Normal singular extension
            else if (singularScore < singularBeta)
                // TT move is probably better than all others, so extend its search by 1 ply
                extension = 1;
            // Negative extension
            else if (ttEntry->score >= beta)
                // some other move is probably better than TT move, so reduce TT move search by 2 plies
                extension = -2;
        }
        // Check extension if no singular extensions
        else if (board.inCheck())
            extension = 1;
        // 7th-rank-pawn extension
        else if (pieceType == (int)PieceType::PAWN 
        && (squareRank(targetSquare) == Rank::RANK_2 || squareRank(targetSquare) == Rank::RANK_7))
            extension = 1;

        skipExtensions:

        // PVS (Principal variation search)
        
        i16 score = 0, searchDepth = depth - 1 + extension;
        
        // LMR (Late move reductions)
        if (legalMovesPlayed > 1 && depth >= LMR_MIN_DEPTH && historyMoveOrLosing)
        {
            //lmr -= board.inCheck(); // reduce checks less
            lmr -= pvNode; // reduce pv nodes less

            // reduce quiets with good history less and vice versa
            if (isQuietMove) 
                lmr -= round((moveScore - HISTORY_MOVE_BASE_SCORE) / (double)LMR_HISTORY_DIVISOR);

            // if lmr is negative, we would have an extension instead of a reduction
            // dont reduce into qsearch
            lmr = clamp(lmr, 0, searchDepth - 1);

            // Reduced search on null window
            score = -search(searchDepth - lmr, ply + 1, -alpha-1, -alpha, true, doubleExtensionsLeft);

            // If score is better than expected (score > alpha), do full depth search on null window
            if (score > alpha && lmr != 1)
                score = -search(searchDepth, ply + 1, -alpha-1, -alpha, !cutNode, doubleExtensionsLeft);
        }
        else if (!pvNode || legalMovesPlayed > 1)
            // Full depth search on null window
            score = -search(searchDepth, ply + 1, -alpha-1, -alpha, !cutNode, doubleExtensionsLeft);

        // Full depth search on full window for some pv nodes
        if (pvNode && (legalMovesPlayed == 1 || score > alpha))
            score = -search(searchDepth, ply + 1, -beta, -alpha, false, doubleExtensionsLeft);

        searchingDone:

        board.undoMove();
        if (isHardTimeUp(nodes)) return 0;

        if (ply == 0)
            movesNodes[move.getMoveEncoded()] += nodes - prevNodes;

        if (score > bestScore) bestScore = score;


        if (score <= alpha) // Fail low
        {
            // Fail low quiets at beginning of array, fail low captures at the end
            if (isQuietMove)
                failLowsHistoryEntry[numFailLowQuiets++] = &(historyTable[stm][pieceType][targetSquare]);
            else if (isCapture)
                failLowsHistoryEntry[256 - ++numFailLowCaptures] = &(historyTable[stm][pieceType][targetSquare]);

            continue;
        }

        alpha = score;
        bestMove = move;

        if (pvNode)
        {
            // Update pv line
            int subPvLineLength = pvLengths[ply + 1];
            pvLengths[ply] = 1 + subPvLineLength;
            pvLines[ply][0] = move;
            memcpy(&(pvLines[ply][1]), pvLines[ply + 1], subPvLineLength * sizeof(Move)); // dst, src, size
        }

        if (score < beta) continue;

        // Fail high / Beta cutoff

        i32 historyBonus = min(HISTORY_MIN_BONUS, HISTORY_BONUS_MULTIPLIER * (depth - 1));

        if (isQuietMove)
        {
            // This quiet move is a killer move and a countermove
            killerMoves[ply] = move;
            if (board.getLastMove() != MOVE_NONE)
                countermoves[stm][board.getLastMove().getMoveEncoded()] = move;

            // Increase this quiet's history
            historyTable[stm][pieceType][targetSquare].updateQuietHistory(board, historyBonus, HISTORY_MAX);

            // Penalize/decrease history of quiets that failed low
            for (int i = 0; i < numFailLowQuiets; i++)
                failLowsHistoryEntry[i]->updateQuietHistory(board, -historyBonus, HISTORY_MAX); 
        }
        else if (isCapture)
        {
            // Increase this capture's history
            historyTable[stm][pieceType][targetSquare].updateCaptureHistory(board, historyBonus, HISTORY_MAX);

            // Penalize/decrease history of captures that failed low
            for (int i = 255, j = 0; j < numFailLowCaptures; i--, j++)
                failLowsHistoryEntry[i]->updateCaptureHistory(board, -historyBonus, HISTORY_MAX);
        }

        break; // Fail high / Beta cutoff
    }

    if (legalMovesPlayed == 0) 
        // checkmate or stalemate
        return board.inCheck() ? NEG_INFINITY + ply : 0;

    if (!singular)
        storeInTT(ttEntry, board.getZobristHash(), depth, bestScore, bestMove, ply, originalAlpha, beta);    

    return bestScore;
}

inline i16 qSearch(int ply, i16 alpha, i16 beta)
{
    // Quiescence search: search captures until a 'quiet' position is reached

    // Update seldepth
    if (ply > maxPlyReached) maxPlyReached = ply;

    if (board.isDraw()) return 0;

    if (ply >= MAX_DEPTH) 
        return board.inCheck() ? 0 : evaluate();

    i16 eval = NEG_INFINITY; // eval is NEG_INFINITY in check
    if (!board.inCheck())
    {
        eval = evaluate();
        if (eval >= beta) return eval;
        if (eval > alpha) alpha = eval;
    }

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), 0, ply, alpha, beta);
    if (shouldCutoff) return ttEntry->adjustedScore(ply);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash ? ttEntry->bestMove : MOVE_NONE;

    // if in check, generate and score all moves, else only captures
    MovesList moves = board.pseudolegalMoves(!board.inCheck()); 
    auto movesScores = scoreMoves(moves, ttMove, killerMoves[ply], board.inCheck());
    
    int legalMovesPlayed = 0;
    i16 bestScore = eval;
    Move bestMove = MOVE_NONE;
    i16 originalAlpha = alpha;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // SEE pruning (skip bad captures)
        if (!board.inCheck() && !SEE(board, move)) continue;

        // skip illegal moves
        if (!board.makeMove(move)) continue; 

        nodes++;
        legalMovesPlayed++;

        i16 score = -qSearch(ply + 1, -beta, -alpha);
        board.undoMove();

        if (score <= bestScore) continue;
        bestScore = score;
        bestMove = move;

        if (bestScore >= beta) break;
        if (bestScore > alpha) alpha = bestScore;
    }

    if (board.inCheck() && legalMovesPlayed == 0) 
        // checkmate
        return NEG_INFINITY + ply; 

    storeInTT(ttEntry, board.getZobristHash(), 0, bestScore, bestMove, ply, originalAlpha, beta);    

    return bestScore;
}

inline array<i32, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove, bool doSEE)
{
    array<i32, 256> movesScores;
    int stm = (int)board.sideToMove();

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();
        PieceType pieceTypeCaptured = board.captured(move);

        if (move == ttMove)
            movesScores[i] = TT_MOVE_SCORE;
        else if (pieceTypeCaptured != PieceType::NONE)
        {
            movesScores[i] = MVV_VALUES[(int)pieceTypeCaptured];
            movesScores[i] += historyTable[stm][pieceType][targetSquare].captureHistory;
            if (doSEE) movesScores[i] += SEE(board, move) ? GOOD_CAPTURE_BASE_SCORE : BAD_CAPTURE_BASE_SCORE;
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            movesScores[i] = PROMOTION_BASE_SCORE + move.typeFlag();
        else if (move == killerMove)
            movesScores[i] = KILLER_SCORE;
        else if (board.getLastMove() != MOVE_NONE && move == countermoves[stm][board.getLastMove().getMoveEncoded()])
            movesScores[i] = COUNTERMOVE_SCORE;
        else
        {
            movesScores[i] = HISTORY_MOVE_BASE_SCORE;
            movesScores[i] += historyTable[stm][pieceType][targetSquare].quietHistory(board);
        }
    }

    return movesScores;
}
