#pragma once

// clang-format off

const uint8_t MAX_DEPTH = 100;

const int32_t PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

const int32_t TT_MOVE_SCORE = MAX_INT32,
              GOOD_CAPTURE_BASE_SCORE = 1'500'000'000,
              PROMOTION_BASE_SCORE = 1'000'000'000,
              KILLER_SCORE = 500'000'000,
              COUNTERMOVE_SCORE = 250'000'000,
              HISTORY_MOVE_BASE_SCORE = 0,
              BAD_CAPTURE_BASE_SCORE = -500'000'000;

const int ASPIRATION_MIN_DEPTH = 6,
          ASPIRATION_INITIAL_DELTA = 25;
const double ASPIRATION_DELTA_MULTIPLIER = 1.5;

const int IIR_MIN_DEPTH = 4;

const int RFP_MAX_DEPTH = 8,
          RFP_DEPTH_MULTIPLIER = 75;

const int NMP_MIN_DEPTH = 3,
          NMP_BASE_REDUCTION = 3,
          NMP_REDUCTION_DIVISOR = 3;

const int FP_MAX_DEPTH = 7,
          FP_BASE = 120,
          FP_MULTIPLIER = 65;

const int LMP_MAX_DEPTH = 8,
          LMP_MIN_MOVES_BASE = 3;
const double LMP_DEPTH_MULTIPLIER = 0.75;

const int SEE_PRUNING_MAX_DEPTH = 9,
          SEE_PRUNING_NOISY_THRESHOLD = -20,
          SEE_PRUNING_QUIET_THRESHOLD = -65;

const int32_t HISTORY_MIN_BONUS = 1570,
              HISTORY_BONUS_MULTIPLIER = 370,
              HISTORY_MAX = 16384;

const int SINGULAR_MIN_DEPTH = 8,
          SINGULAR_DEPTH_MARGIN = 1,
          SINGULAR_BETA_DEPTH_MULTIPLIER = 3,
          SINGULAR_BETA_MARGIN = 24,
          SINGULAR_MAX_DOUBLE_EXTENSIONS = 10;

const int LMR_MIN_DEPTH = 1;
const double LMR_BASE = 1,
             LMR_MULTIPLIER = 0.5;
const int LMR_HISTORY_DIVISOR = 8192;

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
int lmrTable[MAX_DEPTH][256];           // [depth][moveIndex]
int doubleExtensions[MAX_DEPTH+1];      // [ply]
Move killerMoves[MAX_DEPTH];            // [ply]
Move counterMoves[2][1ULL << 16];       // [color][move]
HistoryEntry historyTable[2][6][64];    // [color][pieceType][targetSquare]

namespace uci
{
    inline void info(int depth, int16_t score); 
}

inline void initSearch()
{
    // init lmrTable
    for (int depth = 0; depth < MAX_DEPTH; depth++)
        for (int move = 0; move < 256; move++)
            lmrTable[depth][move] = depth == 0 || move == 0 ? 0 : round(LMR_BASE + ln(depth) * ln(move) * LMR_MULTIPLIER);
}

// Most valuable victim, least valuable attacker
inline int32_t MVVLVA(Move move)
{
    PieceType captured = board.captured(move);
    PieceType capturing = board.pieceTypeAt(move.from());
    return 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing];
}

inline int16_t qSearch(int16_t alpha, int16_t beta, int plyFromRoot)
{
    // Quiescence search: search captures until a 'quiet' position is reached

    // Update seldepth
    if (plyFromRoot > maxPlyReached) maxPlyReached = plyFromRoot;

    int16_t eval = clamp(nnue::evaluate(board.sideToMove()), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
    if (eval >= beta) return eval;
    if (eval > alpha) alpha = eval;

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), 0, alpha, beta, plyFromRoot);
    if (shouldCutoff) return ttEntry->adjustedScore(plyFromRoot);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash ? ttEntry->bestMove : NULL_MOVE;
    MovesList moves = board.pseudolegalMoves(true); // arg = true => captures only

    // Score moves
    array<int32_t, 256> movesScores;
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        if (move == ttMove)
            movesScores[i] = TT_MOVE_SCORE;
        else 
            movesScores[i] = MVVLVA(move);
    }
    
    int16_t bestScore = eval;
    Move bestMove = NULL_MOVE;
    int16_t originalAlpha = alpha;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // SEE pruning (skip bad captures)
        if (!SEE(board, move)) continue;

        // skip illegal moves
        if (!board.makeMove(move)) continue; 

        nodes++;
        int16_t score = -qSearch(-beta, -alpha, plyFromRoot + 1);
        board.undoMove();

        if (score <= bestScore) continue;
        bestScore = score;
        bestMove = move;

        if (bestScore >= beta) break;
        if (bestScore > alpha) alpha = bestScore;
    }

    storeInTT(ttEntry, board.getZobristHash(), 0, bestMove, bestScore, plyFromRoot, originalAlpha, beta);

    return bestScore;
}

inline int16_t PVS(int depth, int16_t alpha, int16_t beta, int plyFromRoot, bool cutNode, Move singularMove = NULL_MOVE, int16_t eval = 0)
{
    assert(plyFromRoot <= MAX_DEPTH);

    if (isHardTimeUp()) return 0;

    // Update seldepth
    if (plyFromRoot > maxPlyReached) maxPlyReached = plyFromRoot;

    pvLengths[plyFromRoot] = 0; // Ensure fresh PV

    if (plyFromRoot > 0 && board.isDraw())
        // Instead of returning 0, introduce very slight randomness to drawn positions
        return -1 + (nodes % 3);

    // Quiescence search at terminal nodes
    if (plyFromRoot >= MAX_DEPTH) return qSearch(alpha, beta, plyFromRoot);

    bool inCheck = board.inCheck();

    // Clamp depth, check extension
    depth = clamp(depth + inCheck, (int)inCheck, (int)MAX_DEPTH); 

    // Quiescence search at terminal nodes
    if (depth <= 0) return qSearch(alpha, beta, plyFromRoot);

    // Probe TT
    auto [ttEntry, shouldCutoff] = probeTT(board.getZobristHash(), depth, alpha, beta, plyFromRoot, singularMove);
    if (shouldCutoff) return ttEntry->adjustedScore(plyFromRoot);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash && singularMove == NULL_MOVE
                ? ttEntry->bestMove : NULL_MOVE;

    int stm = board.sideToMove();
    bool pvNode = beta - alpha > 1 || plyFromRoot == 0;

    // We don't use eval in check because it's unreliable, so don't bother calculating it if in check
    // In singular search we already have the eval, passed in the eval arg
    if (!inCheck && singularMove == NULL_MOVE)
        eval = clamp(nnue::evaluate(stm), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);

    if (!pvNode && !inCheck)
    {
        // RFP (Reverse futility pruning) / Static NMP
        if (depth <= RFP_MAX_DEPTH && eval >= beta + RFP_DEPTH_MULTIPLIER * depth)
            return eval;

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && board.getLastMove() != NULL_MOVE && singularMove == NULL_MOVE
        && board.hasNonPawnMaterial(stm) && eval >= beta)
        {
            board.makeNullMove();
            int nmpDepth = depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR - min((eval - beta)/200, 3);
            int16_t score = -PVS(nmpDepth, -beta, -alpha, plyFromRoot + 1, !cutNode);
            board.undoNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    bool trySingular = singularMove == NULL_MOVE && depth >= SINGULAR_MIN_DEPTH && plyFromRoot > 0
                       && ttMove != NULL_MOVE && abs(ttEntry->score) < MIN_MATE_SCORE
                       && ttEntry->depth >= depth - SINGULAR_DEPTH_MARGIN && ttEntry->getBound() != UPPER_BOUND;
                       
    // IIR (Internal iterative reduction)
    if (ttMove == NULL_MOVE && depth >= IIR_MIN_DEPTH && !inCheck && (pvNode || cutNode))
        depth--;

    MovesList moves = board.pseudolegalMoves();

    // Score moves
    array<int32_t, 256> movesScores;
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        if (move == ttMove)
            movesScores[i] = TT_MOVE_SCORE;
        else if (board.isCapture(move))
        {
            movesScores[i] = SEE(board, move) ? GOOD_CAPTURE_BASE_SCORE : BAD_CAPTURE_BASE_SCORE;
            movesScores[i] += MVVLVA(move);
        }
        else if (move.promotionPieceType() != PieceType::NONE)
            movesScores[i] = PROMOTION_BASE_SCORE + move.typeFlag();
        else if (move == killerMoves[plyFromRoot])
            movesScores[i] = KILLER_SCORE;
        else if (move == counterMoves[board.oppSide()][board.getLastMove().getMove()])
            movesScores[i] = COUNTERMOVE_SCORE;
        else
        {
            int pieceType = (int)board.pieceTypeAt(move.from());
            int targetSquare = (int)move.to();
            movesScores[i] = HISTORY_MOVE_BASE_SCORE + historyTable[stm][pieceType][targetSquare].totalHistory(board);
        }
    }

    int legalMovesPlayed = 0;
    int16_t bestScore = NEG_INFINITY;
    Move bestMove = NULL_MOVE;
    int16_t originalAlpha = alpha;
    HistoryEntry *pointersFailLowQuietsHistoryEntry[256];
    int numFailLowQuiets = 0;
    doubleExtensions[plyFromRoot + 1] = doubleExtensions[plyFromRoot];

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // don't search singular move in singular search
        if (move == singularMove) continue;

        bool historyMoveOrLosing = moveScore < COUNTERMOVE_SCORE;
        int lmr = lmrTable[depth][legalMovesPlayed + 1];
        bool isQuietMove = !board.isCapture(move) && move.promotionPieceType() == PieceType::NONE;

        // Moves loop pruning
        if (plyFromRoot > 0 && historyMoveOrLosing && bestScore > -MIN_MATE_SCORE)
        {
            // LMP (Late move pruning)
            if (depth <= LMP_MAX_DEPTH 
            && legalMovesPlayed >= LMP_MIN_MOVES_BASE + pvNode + inCheck + depth * depth * LMP_DEPTH_MULTIPLIER)
                break;

            // FP (Futility pruning)
            if (depth <= FP_MAX_DEPTH && !inCheck && alpha < MIN_MATE_SCORE 
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

        // SE (Singular extension)
        int extension = 0;
        if (trySingular && move == ttEntry->bestMove)
        {
            // Singular search: before searching any move, search this node at a shallower depth with TT move excluded
            board.undoMove(); // undo TT move we just made
            int16_t singularBeta = ttEntry->score - depth * SINGULAR_BETA_DEPTH_MULTIPLIER;
            int16_t singularScore = PVS((depth - 1) / 2, singularBeta - 1, singularBeta, plyFromRoot, cutNode, move, eval);
            board.makeMove(move, false); // second arg = false => don't check legality (we already verified it's a legal move)

            if (!pvNode && singularScore < singularBeta && singularScore < singularBeta - SINGULAR_BETA_MARGIN 
            && doubleExtensions[plyFromRoot + 1] < SINGULAR_MAX_DOUBLE_EXTENSIONS)
            {
                // singularScore is way lower than TT score
                // TT move is probably MUCH better than all others, so extend its search by 2 plies
                extension = 2;
                doubleExtensions[plyFromRoot + 1]++; // Keep track of these double extensions so search doesn't explode
            }
            else if (singularScore < singularBeta)
                // TT move is probably better than all others, so extend its search by 1 ply
                extension = 1;
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
            score = -PVS(depth - 1 + extension, -beta, -alpha, plyFromRoot + 1, false);
            goto searchingDone;
        }
        
        // LMR (Late move reductions)
        if (depth >= LMR_MIN_DEPTH && historyMoveOrLosing)
        {
            lmr -= board.inCheck(); // reduce checks less
            lmr -= pvNode;          // reduce pv nodes less

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
        score = -PVS(depth - 1 - lmr, -alpha - 1, -alpha, plyFromRoot + 1, true);
        if (score > alpha && (score < beta || lmr > 0))
            score = -PVS(depth - 1, -beta, -alpha, plyFromRoot + 1, score < beta ? false : !cutNode);

        searchingDone:

        board.undoMove();
        if (isHardTimeUp()) return 0;

        if (plyFromRoot == 0)
            movesNodes[move.getMove()] += nodes - prevNodes;

        if (score > bestScore) bestScore = score;

        if (score <= alpha) // Fail low
        {
            if (isQuietMove)
            {
                int pieceType = (int)board.pieceTypeAt(move.from());
                int targetSquare = (int)move.to();
                pointersFailLowQuietsHistoryEntry[numFailLowQuiets++] = &(historyTable[stm][pieceType][targetSquare]);
            }
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

        if (!isQuietMove) break;

        // This quiet move is a killer move and a countermove
        killerMoves[plyFromRoot] = move;
        counterMoves[(int)board.oppSide()][board.getLastMove().getMove()] = move;

        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();

        // Increase this quiet's history
        int32_t historyBonus = min(HISTORY_MIN_BONUS, HISTORY_BONUS_MULTIPLIER * (depth - 1));
        historyTable[stm][pieceType][targetSquare].updateHistory(board, historyBonus);

        // Penalize/decrease history of quiets that failed low
        for (int i = 0; i < numFailLowQuiets; i++)
            pointersFailLowQuietsHistoryEntry[i]->updateHistory(board, -historyBonus); 

        break; // Fail high / Beta cutoff
    }

    if (legalMovesPlayed == 0) 
        // checkmate or stalemate
        return inCheck ? NEG_INFINITY + plyFromRoot : 0;

    storeInTT(ttEntry, board.getZobristHash(), depth, bestMove, bestScore, plyFromRoot, originalAlpha, beta);    

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
        score = PVS(depth, alpha, beta, 0, false);

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
                : PVS(iterationDepth, NEG_INFINITY, POS_INFINITY, 0, false);

        if (isHardTimeUp()) break;

        uci::info(iterationDepth, score);

        double bestMoveNodesFraction = (double)movesNodes[pvLines[0][0].getMove()] / (double)nodes;
        if (isSoftTimeUp(bestMoveNodesFraction)) break;
    }

    if (ttAge < 63) ttAge++;

    return pvLines[0][0]; // return best move
}

