#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <chrono>
#include <climits>
#include <cmath>
#include "chess.hpp"
#include "eval.hpp"
#include "see.hpp"
using namespace chess;
using namespace std;

const int POS_INFINITY = 9999999, NEG_INFINITY = -9999999;
Move NULL_MOVE;

const char EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    U64 key;
    int score;
    Move bestMove;
    int depth;
    char type;
};
TTEntry TT[0x800000];

chrono::steady_clock::time_point start;
float timeForThisTurn;
Board board;
Move bestMoveRoot, bestMoveRootAsp;
int globalScore;
Move killerMoves[512][2];

inline bool isTimeUp()
{
    return (chrono::steady_clock::now() - start) / std::chrono::milliseconds(1) >= timeForThisTurn;
}

inline void scoreMoves(Movelist &moves, int *scores, U64 boardKey, U64 indexInTT, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        if (TT[indexInTT].key == boardKey && moves[i] == TT[indexInTT].bestMove)
            scores[i] = INT_MAX;
        else if (board.isCapture(moves[i]))
        {
            PieceType captured = board.at<PieceType>(moves[i].to());
            PieceType capturing = board.at<PieceType>(moves[i].from());
            int moveScore = 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing]; // MVVLVA
            moveScore += SEE(board, moves[i]) ? 100'000'000 : -850'000'000;
            scores[i] = moveScore;
        }
        else if (moves[i].typeOf() == moves[i].PROMOTION)
            scores[i] = -400'000'000;
        else if (killerMoves[plyFromRoot][0] == moves[i] || killerMoves[plyFromRoot][1] == moves[i])
            scores[i] = -700'000'000;
        else
            scores[i] = -1'000'000'000;
    }
}

inline int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence saarch: search capture moves until a 'quiet' position is reached

    int eval = evaluate(board);
    if (eval >= beta)
        return eval;

    if (alpha < eval)
        alpha = eval;

    Movelist moves;
    movegen::legalmoves<MoveGenType::CAPTURE>(moves, board);
    U64 boardKey = board.zobrist();
    U64 indexInTT = 0x7FFFFF & boardKey;
    int scores[218];
    scoreMoves(moves, scores, boardKey, indexInTT, plyFromRoot);
    int bestScore = eval;

    for (int i = 0; i < moves.size(); i++)
    {
        // sort moves incrementally
        for (int j = i + 1; j < moves.size(); j++)
            if (scores[j] > scores[i])
            {
                swap(moves[i], moves[j]);
                swap(scores[i], scores[j]);
            }
        Move capture = moves[i];

        board.makeMove(capture);
        int score = -qSearch(-beta, -alpha, plyFromRoot + 1);
        board.unmakeMove(capture);

        if (score > bestScore)
            bestScore = score;
        else
            continue;

        if (score >= beta)
            return score; // in CPW: return beta;
        if (score > alpha)
            alpha = score;
    }

    return bestScore;
}

inline void savePotentialKillerMove(Move move, int plyFromRoot)
{
    if (!board.isCapture(move) && move != killerMoves[plyFromRoot][0])
    {
        killerMoves[plyFromRoot][1] = killerMoves[plyFromRoot][0];
        killerMoves[plyFromRoot][0] = move;
    }
}

inline int search(int depth, int plyFromRoot, int alpha, int beta, bool doNull = true)
{
    if (plyFromRoot > 0 && board.isRepetition())
        return 0;

    int originalAlpha = alpha;
    bool inCheck = board.inCheck();
    if (inCheck)
        depth++;

    U64 boardKey = board.zobrist();
    U64 indexInTT = 0x7FFFFF & boardKey;
    if (plyFromRoot > 0 && TT[indexInTT].key == boardKey && TT[indexInTT].depth >= depth)
        if (TT[indexInTT].type == EXACT || (TT[indexInTT].type == LOWER_BOUND && TT[indexInTT].score >= beta) || (TT[indexInTT].type == UPPER_BOUND && TT[indexInTT].score <= alpha))
            return TT[indexInTT].score;

    // Internal iterative reduction
    if (TT[indexInTT].key != boardKey && depth > 3)
        depth--;

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty())
        return inCheck ? NEG_INFINITY + plyFromRoot : 0; // else it's draw

    if (depth <= 0)
        return qSearch(alpha, beta, plyFromRoot);

    // RFP (Reverse futility pruning)
    bool pv = beta - alpha > 1;
    if (!pv && plyFromRoot > 0 && depth <= 7 && !inCheck)
    {
        int eval = evaluate(board);
        if (eval >= beta + 53 * depth)
            return eval;
    }

    // NMP (Null move pruning)
    if (!pv && plyFromRoot > 0 && depth >= 3 && !inCheck && doNull)
    {
        bool hasAtLeast1Piece = board.pieces(PieceType::KNIGHT, board.sideToMove()) > 0 || board.pieces(PieceType::BISHOP, board.sideToMove()) > 0 || board.pieces(PieceType::ROOK, board.sideToMove()) > 0 || board.pieces(PieceType::QUEEN, board.sideToMove()) > 0;
        if (hasAtLeast1Piece)
        {
            board.makeNullMove();
            int eval = -search(depth - 3, plyFromRoot + 1, -beta, -alpha, false);
            board.unmakeNullMove();
            if (eval >= beta)
                return eval;
            if (isTimeUp())
                return POS_INFINITY;
        }
    }

    int scores[218];
    scoreMoves(moves, scores, boardKey, indexInTT, plyFromRoot);
    int bestEval = NEG_INFINITY;
    Move bestMove;
    bool canLmr = depth > 1 && plyFromRoot > 0 && !inCheck;

    for (int i = 0; i < moves.size(); i++)
    {
        // sort moves incrementally
        for (int j = i + 1; j < moves.size(); j++)
            if (scores[j] > scores[i])
            {
                swap(moves[i], moves[j]);
                swap(scores[i], scores[j]);
            }
        Move move = moves[i];
        board.makeMove(move);

        // PVS (Principal variation search)
        int eval;
        if (i == 0)
            eval = -search(depth - 1, plyFromRoot + 1, -beta, -alpha);
        else
        {
            // LMR (Late move reduction)
            int lmr = 0;
            if (canLmr)
            {
                lmr = 0.77 + log(depth) * log(i) / 2.36; // ln(x)
                lmr -= board.inCheck();                  // reduce checks less
                lmr -= pv;                               // reduce pv nodes less
                if (lmr < 0)
                    lmr = 0;
            }
            eval = -search(depth - 1 - lmr, plyFromRoot + 1, -alpha - 1, -alpha);
            if (eval > alpha && (eval < beta || lmr > 0))
                eval = -search(depth - 1, plyFromRoot + 1, -beta, -alpha);
        }

        board.unmakeMove(move);

        if (eval > bestEval)
        {
            bestEval = eval;
            bestMove = move;
            if (plyFromRoot == 0)
                bestMoveRoot = move;

            if (bestEval > alpha)
                alpha = bestEval;
            if (alpha >= beta)
            {
                savePotentialKillerMove(move, plyFromRoot);
                break;
            }
        }

        if (isTimeUp())
            return POS_INFINITY;
    }

    TT[indexInTT].key = boardKey;
    TT[indexInTT].depth = depth;
    TT[indexInTT].score = bestEval;
    TT[indexInTT].bestMove = bestMove;
    if (bestEval <= originalAlpha)
        TT[indexInTT].type = UPPER_BOUND;
    else if (bestEval >= beta)
        TT[indexInTT].type = LOWER_BOUND;
    else
        TT[indexInTT].type = EXACT;

    return bestEval;
}

inline void aspiration(int maxDepth)
{
    int delta = 25;
    int alpha = max(NEG_INFINITY, globalScore - delta);
    int beta = min(POS_INFINITY, globalScore + delta);
    int depth = maxDepth;

    while (!isTimeUp())
    {
        globalScore = search(depth, 0, alpha, beta);
        if (isTimeUp())
            break;

        if (globalScore >= beta)
        {
            beta = min(beta + delta, POS_INFINITY);
            bestMoveRootAsp = bestMoveRoot;
            depth--;
        }
        else if (globalScore <= alpha)
        {
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, NEG_INFINITY);
            depth = maxDepth;
        }
        else
            break;

        delta *= 2;
    }
}

inline int iterativeDeepening(float millisecondsLeft)
{
    start = chrono::steady_clock::now();
    timeForThisTurn = millisecondsLeft / (float)30;
    bestMoveRootAsp = NULL_MOVE;
    int iterationDepth = 1;
    while (!isTimeUp())
    {
        if (iterationDepth < 6)
            globalScore = search(iterationDepth, 0, NEG_INFINITY, POS_INFINITY);
        else
            aspiration(iterationDepth);
        iterationDepth++;
    }
    return iterationDepth;
}

#endif