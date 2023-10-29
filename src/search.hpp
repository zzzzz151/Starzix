#pragma once

// clang-format off

// ----- Tunable params ------

const int MAX_DEPTH = 100;

const int PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

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

const int SEE_PRUNING_NOISY_THRESHOLD = -90,
          SEE_PRUNING_QUIET_THRESHOLD = -50;

const int HISTORY_MIN_BONUS = 1570,
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

// ----- End tunable params -----

// ----- Global vars -----

const int POS_INFINITY = 9999999, NEG_INFINITY = -POS_INFINITY, MIN_MATE_SCORE = POS_INFINITY - 512;
Board board;
Move pvLines[MAX_DEPTH * 2][MAX_DEPTH * 2];
int pvLengths[MAX_DEPTH * 2];
uint64_t nodes;
uint64_t movesNodes[64][64]; // from, to
int maxPlyReached;
Move killerMoves[MAX_DEPTH * 3][2]; // ply, killerIndex
int history[2][6][64]; // color, pieceType, targetSquare
Move counterMoves[2][1ULL << 16]; // color, moveEncoded
int lmrTable[MAX_DEPTH * 2][256]; // depth, move
int doubleExtensions[MAX_DEPTH * 2]; // ply

// ----- End global vars -----

#include "time_management.hpp"
#include "tt.hpp"
#include "see.hpp"
#include "move_scoring.hpp"
#include "nnue.hpp"

namespace uci
{
    inline void info(int depth, int score); 
}

inline void initLmrTable()
{
    for (int depth = 0; depth < MAX_DEPTH * 2; depth++)
        for (int move = 0; move < 256; move++)
            // log(x) is ln(x)
            lmrTable[depth][move] = depth == 0 || move == 0 ? 0 : round(LMR_BASE + log(depth) * log(move) * LMR_MULTIPLIER);
}

inline int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence search: search capture moves until a 'quiet' position is reached

    if (plyFromRoot > maxPlyReached)
        maxPlyReached = plyFromRoot;

    int eval = nnue::evaluate(board.sideToMove());
    if (eval >= beta) 
        return eval;
    if (alpha < eval) 
        alpha = eval;

    // probe TT
    TTEntry *ttEntry = &(tt[board.getZobristHash() % tt.size()]);
    if (plyFromRoot > 0 && board.getZobristHash() == ttEntry->zobristHash
    && (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha)))
    {
        int score = ttEntry->score;
        if (abs(score) >= MIN_MATE_SCORE) 
            score += score < 0 ? plyFromRoot : -plyFromRoot;
        return score;
    }

    MovesList moves = board.pseudolegalMoves(true);
    int scores[256];
    scoreMoves(moves, scores, *ttEntry, plyFromRoot);
    int bestScore = eval;
    Move bestMove = NULL_MOVE;
    int originalAlpha = alpha;

    for (int i = 0; i < moves.size(); i++)
    {
        Move capture = incrementalSort(moves, scores, i);

        // SEE pruning (skip bad captures)
        if (!SEE(board, capture)) 
            continue;

        if (!board.makeMove(capture))
            continue; // illegal move

        nodes++;
        int score = -qSearch(-beta, -alpha, plyFromRoot + 1);
        board.undoMove();

        if (score <= bestScore)
            continue;

        bestScore = score;

        if (score >= beta) 
        {
            bestMove = capture;
            break;
        }
        if (score > alpha) 
        {
            alpha = score;
            bestMove = capture;
        }
    }

    if (ttEntry->depth <= 0)
        storeInTT(ttEntry, 0, bestMove, bestScore, plyFromRoot, originalAlpha, beta);

    return bestScore;
}

inline int search(int depth, int alpha, int beta, int plyFromRoot, bool skipNmp = false, Move singularMove = NULL_MOVE)
{
    if (isHardTimeUp())
        return 0;

    if (plyFromRoot > maxPlyReached) 
        maxPlyReached = plyFromRoot;

    pvLengths[plyFromRoot] = 0;

    if (plyFromRoot > 0 && (board.isDraw() || board.isRepetition()))
        return -1 + (nodes % 3);

    bool inCheck = board.inCheck();
    if (inCheck) depth++; // Check extension

    if (depth <= 0) 
        return qSearch(alpha, beta, plyFromRoot);

    // Probe TT
    TTEntry *ttEntry = &(tt[board.getZobristHash() % tt.size()]);
    if (plyFromRoot > 0 && ttEntry->zobristHash == board.getZobristHash() && ttEntry->depth >= depth && singularMove == NULL_MOVE
    && (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha)))
    {
        int score = ttEntry->score;
        if (abs(score) >= MIN_MATE_SCORE) 
            score += score < 0 ? plyFromRoot : -plyFromRoot;
        return score;
    }

    // IIR (Internal iterative reduction)
    if (ttEntry->zobristHash != board.getZobristHash() && depth >= IIR_MIN_DEPTH && !inCheck)
        depth--;

    bool pvNode = beta - alpha > 1 || plyFromRoot == 0;
    int eval = nnue::evaluate(board.sideToMove());

    if (!pvNode && !inCheck)
    {
        // RFP (Reverse futility pruning)
        if (depth <= RFP_MAX_DEPTH && eval >= beta + RFP_DEPTH_MULTIPLIER * depth)
            return eval;

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && !skipNmp && eval >= beta && board.hasNonPawnMaterial(board.sideToMove()))
        {
            board.makeNullMove();
            int nmpDepth = depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR - min((eval - beta)/200, 3);
            int score = -search(nmpDepth, -beta, -alpha, plyFromRoot + 1, true);
            board.undoNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    bool trySingular = singularMove == NULL_MOVE && depth >= SINGULAR_MIN_DEPTH && plyFromRoot > 0
                       && ttEntry->zobristHash == board.getZobristHash() && ttEntry->depth >= depth - SINGULAR_DEPTH_MARGIN
                       && ttEntry->type != UPPER_BOUND && abs(ttEntry->score) < MIN_MATE_SCORE;

    MovesList moves = board.pseudolegalMoves();
    int scores[256];
    scoreMoves(moves, scores, *ttEntry, plyFromRoot);
    int bestScore = NEG_INFINITY;
    Move bestMove;
    int originalAlpha = alpha;
    int legalMovesPlayed = 0;
    int stm = board.sideToMove();
    int* pointersFailLowQuietsHistory[256], numFailLowQuiets = 0;
    doubleExtensions[plyFromRoot + 1] = doubleExtensions[plyFromRoot];

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = incrementalSort(moves, scores, i);

        // don't search singular move in singular search
        if (move == singularMove)
            continue;

        bool historyMoveOrLosing = scores[i] < KILLER_SCORE;
        int lmr = lmrTable[depth][legalMovesPlayed];
        bool isQuietMove = !board.isCapture(move) && move.promotionPieceType() == PieceType::NONE;

        // Moves loop pruning
        if (historyMoveOrLosing && bestScore > -MIN_MATE_SCORE)
        {
            // LMP (Late move pruning)
            if (depth <= LMP_MAX_DEPTH && legalMovesPlayed >= LMP_MIN_MOVES_BASE + pvNode + inCheck + depth * depth)
                break;

            // FP (Futility pruning)
            if (depth <= FP_MAX_DEPTH && alpha < MIN_MATE_SCORE && eval + FP_BASE + max(depth - lmr, 0) * FP_MULTIPLIER <= alpha)
                break;

            // SEE pruning
            int threshold = depth * (isQuietMove ? SEE_PRUNING_QUIET_THRESHOLD : SEE_PRUNING_NOISY_THRESHOLD);
            if (!SEE(board, move, threshold))
                continue;
        }

        if (!board.makeMove(move))
            continue; // illegal move

        uint64_t prevNodes = nodes;
        nodes++;
        legalMovesPlayed++;

        // SE (Singular extension)
        int extension = 0;
        if (trySingular && move == ttEntry->bestMove)
        {
            // Singular search: before searching any move, search this node at a shallower depth with TT move excluded
            board.undoMove(); // undo TT move we just made
            int singularBeta = ttEntry->score - depth * SINGULAR_BETA_DEPTH_MULTIPLIER;
            int singularScore = search((depth - 1) / 2, singularBeta - 1, singularBeta, plyFromRoot, true, move);
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
        }

        Square targetSquare = move.to();
        int pieceType = move.promotionPieceType() != PieceType::NONE ? (int)PieceType::PAWN : (int)board.pieceTypeAt(targetSquare);
        int *pointerMoveHistory = &(history[stm][pieceType][targetSquare]);

        // PVS (Principal variation search)
        int score = 0;
        if (legalMovesPlayed == 1)
            score = -search(depth - 1 + extension, -beta, -alpha, plyFromRoot + 1);
        else
        {
            // LMR (Late move reductions)
            if (depth >= LMR_MIN_DEPTH && historyMoveOrLosing)
            {
                lmr -= board.inCheck(); // reduce checks less
                lmr -= pvNode;          // reduce pv nodes less

                // reduce quiets with good history less and vice versa
                if (isQuietMove) lmr -= round(*pointerMoveHistory / (double)LMR_HISTORY_DIVISOR);

                // if lmr is negative, we would have an extension instead of a reduction
                if (lmr < 0) lmr = 0;
            }
            else
                lmr = 0;

            score = -search(depth - 1 - lmr, -alpha - 1, -alpha, plyFromRoot + 1);
            if (score > alpha && (score < beta || lmr > 0))
                score = -search(depth - 1, -beta, -alpha, plyFromRoot + 1);
        }

        board.undoMove();
        if (isHardTimeUp()) return 0;

        if (plyFromRoot == 0)
            movesNodes[move.from()][targetSquare] += nodes - prevNodes;

        if (score > bestScore)
            bestScore = score;

        if (score <= alpha)
        {
            // Fail low
            if (isQuietMove)
                pointersFailLowQuietsHistory[numFailLowQuiets++] = pointerMoveHistory;
            continue;
        }

        alpha = score;
        bestMove = move;

        if (pvNode)
        {
            // Update pv line
            plyFromRoot++;
            int subPvLineLength = pvLengths[plyFromRoot];
            pvLengths[plyFromRoot - 1] = 1 + subPvLineLength;
            pvLines[plyFromRoot - 1][0] = move;
            memcpy(&(pvLines[plyFromRoot - 1][1]), pvLines[plyFromRoot], subPvLineLength * sizeof(Move)); // dst, src, size
            plyFromRoot--;
        }

        if (score < beta) 
            continue;

        // Fail high / Beta cutoff

        if (isQuietMove && move != killerMoves[plyFromRoot][0])
        {
            killerMoves[plyFromRoot][1] = killerMoves[plyFromRoot][0];
            killerMoves[plyFromRoot][0] = move;
        }

        if (isQuietMove)
        {
            counterMoves[(int)board.oppSide()][board.getLastMove().move()] = move;

            int bonus = min(HISTORY_MIN_BONUS, HISTORY_BONUS_MULTIPLIER * (depth - 1));
            *pointerMoveHistory += bonus - *pointerMoveHistory * bonus / HISTORY_MAX;

            // Penalize quiets that failed low
            for (int i = 0; i < numFailLowQuiets; i++)
                *pointersFailLowQuietsHistory[i] -= bonus + *pointersFailLowQuietsHistory[i] * bonus / HISTORY_MAX;
        }

        break; // Fail high / Beta cutoff
    }

    if (legalMovesPlayed == 0) 
        return inCheck ? NEG_INFINITY + plyFromRoot : 0; // checkmate/draw

    storeInTT(ttEntry, depth, bestMove, bestScore, plyFromRoot, originalAlpha, beta);    

    return bestScore;
}

inline int aspiration(int maxDepth, int score)
{
    int delta = ASPIRATION_INITIAL_DELTA;
    int alpha = max(NEG_INFINITY, score - delta);
    int beta = min(POS_INFINITY, score + delta);
    int depth = maxDepth;

    while (true)
    {
        score = search(depth, alpha, beta, 0);

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

inline void iterativeDeepening()
{
    // clear stuff
    nodes = 0;
    memset(movesNodes, 0, sizeof(movesNodes));
    memset(pvLines, 0, sizeof(pvLines));
    memset(pvLengths, 0, sizeof(pvLengths));
    memset(killerMoves, 0, sizeof(killerMoves));
    memset(counterMoves, 0, sizeof(counterMoves));
    memset(history, 0, sizeof(history));
    memset(doubleExtensions, 0, sizeof(doubleExtensions));

    int iterationDepth = 1, score = 0;
    while (iterationDepth <= MAX_DEPTH)
    {
        maxPlyReached = -1;

        score = iterationDepth >= ASPIRATION_MIN_DEPTH 
                ? aspiration(iterationDepth, score) 
                : search(iterationDepth, NEG_INFINITY, POS_INFINITY, 0);

        if (isHardTimeUp())
            break;

        uci::info(iterationDepth, score);
        iterationDepth++;

        if (isSoftTimeUp())
            break;

    }
}

