#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "chess.hpp"
#include "nnue.hpp"
#include "see.hpp"
inline void sendInfo(int depth, int score);
using namespace chess;
using namespace std;

const int POS_INFINITY = 9999999, NEG_INFINITY = -9999999;
Move NULL_MOVE;

const char EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    U64 key = 0;
    int score;
    Move bestMove;
    int16_t depth;
    char type;
};
int TT_SIZE_MB = 64;
uint32_t NUM_TT_ENTRIES;
vector<TTEntry> TT;

const int PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

U64 nodes;
chrono::steady_clock::time_point start;
double millisecondsForThisTurn;
Board board;
Move bestMoveRoot;
Move killerMoves[512][2];   // ply, move
int historyMoves[2][6][64]; // color, pieceType, squareTo

inline bool isTimeUp()
{
    return (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= millisecondsForThisTurn;
}

inline void scoreMoves(Movelist &moves, int *scores, U64 boardKey, TTEntry &ttEntry, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (ttEntry.key == boardKey && move == ttEntry.bestMove)
            scores[i] = INT_MAX;
        else if (board.isCapture(move))
        {
            PieceType captured = board.at<PieceType>(move.to());
            PieceType capturing = board.at<PieceType>(move.from());
            int moveScore = 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing]; // MVVLVA
            moveScore += SEE(board, move) ? 100'000'000 : -850'000'000;
            scores[i] = moveScore;
        }
        else if (move.typeOf() == move.PROMOTION)
            scores[i] = -400'000'000;
        else if (killerMoves[plyFromRoot][0] == move || killerMoves[plyFromRoot][1] == move)
            scores[i] = -700'000'000;
        else
        {
            int stm = (int)board.sideToMove();
            int pieceType = (int)board.at<PieceType>(move.from());
            int squareTo = (int)move.to();
            scores[i] = -1'000'000'000 + historyMoves[stm][pieceType][squareTo];
        }
    }
}

inline int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence saarch: search capture moves until a 'quiet' position is reached

    int eval = network.Evaluate((int)board.sideToMove());
    if (eval >= beta)
        return eval;

    if (alpha < eval)
        alpha = eval;

    Movelist moves;
    movegen::legalmoves<MoveGenType::CAPTURE>(moves, board);
    U64 boardKey = board.zobrist();
    TTEntry ttEntry = TT[boardKey % NUM_TT_ENTRIES];
    int scores[218];
    scoreMoves(moves, scores, boardKey, ttEntry, plyFromRoot);
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
        nodes++;
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

inline int search(int depth, int plyFromRoot, int alpha, int beta, bool doNull = true)
{
    if (plyFromRoot > 0 && board.isRepetition())
        return 0;

    if (isTimeUp())
        return 0;

    int originalAlpha = alpha;
    bool inCheck = board.inCheck();
    if (inCheck)
        depth++;

    U64 boardKey = board.zobrist();
    TTEntry *ttEntry = &TT[boardKey % NUM_TT_ENTRIES];
    if (plyFromRoot > 0 && ttEntry->key == boardKey && ttEntry->depth >= depth)
        if (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha))
            return ttEntry->score;

    // Internal iterative reduction
    if (ttEntry->key != boardKey && depth > 3)
        depth--;

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty())
        return inCheck ? NEG_INFINITY + plyFromRoot : 0; // else it's draw

    if (depth <= 0)
        return qSearch(alpha, beta, plyFromRoot);

    bool pvNode = beta - alpha > 1 || plyFromRoot == 0;

    if (!pvNode && !inCheck)
    {
        int eval = network.Evaluate((int)board.sideToMove());

        // RFP (Reverse futility pruning)
        if (depth <= 8 && eval >= beta + 75 * depth)
            return eval;

        // NMP (Null move pruning)
        if (depth >= 3 && doNull && eval >= beta)
        {
            bool hasAtLeast1Piece = board.pieces(PieceType::KNIGHT, board.sideToMove()) > 0 || board.pieces(PieceType::BISHOP, board.sideToMove()) > 0 || board.pieces(PieceType::ROOK, board.sideToMove()) > 0 || board.pieces(PieceType::QUEEN, board.sideToMove()) > 0;
            if (hasAtLeast1Piece)
            {
                board.makeNullMove();
                int score = -search(depth - 3 - depth / 3, plyFromRoot + 1, -beta, -alpha, false);
                board.unmakeNullMove();
                if (score >= POS_INFINITY - 256)
                    return beta;
                if (score >= beta)
                    return score;
            }
        }
    }

    int scores[218];
    scoreMoves(moves, scores, boardKey, *ttEntry, plyFromRoot);
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
        nodes++;

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
                lmr -= pvNode;                           // reduce pv nodes less
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
                bool isQuietMove = !board.isCapture(move) && move.typeOf() != move.PROMOTION;
                if (isQuietMove && move != killerMoves[plyFromRoot][0])
                {
                    killerMoves[plyFromRoot][1] = killerMoves[plyFromRoot][0];
                    killerMoves[plyFromRoot][0] = move;
                }
                if (isQuietMove)
                {
                    int stm = (int)board.sideToMove();
                    int pieceType = (int)board.at<PieceType>(move.from());
                    int squareTo = (int)move.to();
                    historyMoves[stm][pieceType][squareTo] += depth * depth;
                }
                break;
            }
        }
    }

    ttEntry->key = boardKey;
    ttEntry->depth = depth;
    ttEntry->score = bestEval;
    ttEntry->bestMove = bestMove;
    if (bestEval <= originalAlpha)
        ttEntry->type = UPPER_BOUND;
    else if (bestEval >= beta)
        ttEntry->type = LOWER_BOUND;
    else
        ttEntry->type = EXACT;

    return bestEval;
}

inline int aspiration(int maxDepth, int score)
{
    int delta = 25;
    int alpha = max(NEG_INFINITY, score - delta);
    int beta = min(POS_INFINITY, score + delta);
    int depth = maxDepth;

    while (true)
    {
        score = search(depth, 0, alpha, beta);

        if (isTimeUp())
            break;

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

        delta *= 1.5;
    }

    return score;
}

inline Move iterativeDeepening(bool info = false)
{
    bestMoveRoot = NULL_MOVE;
    nodes = 0;

    int iterationDepth = 1, score = 0;
    while (true)
    {
        Move before = bestMoveRoot;
        if (iterationDepth < 6)
            score = search(iterationDepth, 0, NEG_INFINITY, POS_INFINITY);
        else
            score = aspiration(iterationDepth, score);

        if (isTimeUp())
        {
            bestMoveRoot = before;
            sendInfo(iterationDepth, score);
            break;
        }

        if (info)
            sendInfo(iterationDepth, score);

        iterationDepth++;
    }

    return bestMoveRoot;
}

#endif