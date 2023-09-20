#ifndef SEARCH_HPP
#define SEARCH_HPP

// clang-format off
#include "chess.hpp"
#include "time_management.hpp"
#include "nnue.hpp"
#include "see.hpp"
inline void info(int depth, int score); // from uci.hpp
using namespace chess;
using namespace std;

// ----- Tunable params ------

const uint8_t MAX_DEPTH = 100;

int TT_SIZE_MB = 64; // default (and current) TT size in MB

extern const int PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0},
                 SEE_PIECE_VALUES[7] = {100, 300, 300, 500, 900, 0, 0},
                 PAWN_INDEX = 0;

const uint8_t ASPIRATION_MIN_DEPTH = 6,
              ASPIRATION_INITIAL_DELTA = 25;
const double ASPIRATION_DELTA_MULTIPLIER = 1.5;

const uint8_t IIR_MIN_DEPTH = 4;

const uint8_t RFP_MAX_DEPTH = 8,
              RFP_DEPTH_MULTIPLIER = 75;

const uint8_t NMP_MIN_DEPTH = 3,
              NMP_BASE_REDUCTION = 3,
              NMP_REDUCTION_DIVISOR = 3;

const uint8_t FP_MAX_DEPTH = 7,
              FP_BASE = 120,
              FP_MULTIPLIER = 65;

const uint8_t LMP_MAX_DEPTH = 8,
              LMP_MIN_MOVES_BASE = 2;

const uint8_t SEE_PRUNING_MAX_DEPTH = 9;
const int SEE_PRUNING_NOISY_THRESHOLD = -90,
          SEE_PRUNING_QUIET_THRESHOLD = -50;

const uint8_t LMR_MIN_DEPTH = 1;
const double LMR_BASE = 1,
             LMR_DIVISOR = 2,
             LMR_HISTORY_DIVISOR = 1024;

// ----- End tunable params -----

// ----- Global vars -----

const int POS_INFINITY = 9999999, NEG_INFINITY = -POS_INFINITY, MIN_MATE_SCORE = POS_INFINITY - 512;
Move NULL_MOVE;
Board board;
Move bestMoveRoot;
U64 nodes;
int maxPlyReached;
Move killerMoves[MAX_DEPTH*2][2]; // ply, move
int historyMoves[2][6][64];       // color, pieceType, squareTo
int lmrTable[MAX_DEPTH + 2][218]; // depth, move (lmrTable initialized in main.cpp)
Move counterMove;

const char INVALID = 0, EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    U64 key = 0;
    int score = 0;
    Move bestMove = NULL_MOVE;
    uint8_t depth = 0;
    char type = INVALID;
};
uint32_t NUM_TT_ENTRIES;
vector<TTEntry> TT;

// ----- End global vars -----

#include "move_scoring.hpp"

inline int search(int depth, int alpha, int beta, int plyFromRoot, bool skipNmp = false);

inline int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence saarch: search capture moves until a 'quiet' position is reached

    int originalAlpha = alpha;
    int eval = network.Evaluate((int)board.sideToMove());
    if (eval >= beta) return eval;
    if (alpha < eval) alpha = eval;

    U64 boardKey = board.zobrist();
    // probe TT
    TTEntry *ttEntry = &(TT[boardKey % NUM_TT_ENTRIES]);
    if (plyFromRoot > 0 && ttEntry->key == boardKey)
        if (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha))
        {
            int ret = ttEntry->score;
            if (abs(ret) >= MIN_MATE_SCORE) ret += ret < 0 ? plyFromRoot : -plyFromRoot;
            return ret;
        }

    Movelist moves;
    movegen::legalmoves<MoveGenType::CAPTURE>(moves, board);

    int scores[218];
    scoreMoves(moves, scores, boardKey, *ttEntry, plyFromRoot);

    int bestScore = eval;
    for (int i = 0; i < moves.size(); i++)
    {
        Move capture = incrementalSort(moves, scores, i);

        // SEE pruning (skip bad captures)
        if (!SEE(board, capture)) continue;

        board.makeMove(capture);
        nodes++;
        int score = -qSearch(-beta, -alpha, plyFromRoot + 1);
        board.unmakeMove(capture);

        if (score > bestScore) bestScore = score;
        else continue;

        if (score >= beta) return score; // in CPW: return beta;
        if (score > alpha) alpha = score;
    }

    if (ttEntry->depth <= 0)
    {
        // save in TT
        ttEntry->key = boardKey;
        ttEntry->depth = 0;
        ttEntry->score = bestScore;
        if (abs(bestScore) >= MIN_MATE_SCORE) ttEntry->score += bestScore < 0 ? -plyFromRoot : plyFromRoot;
        ttEntry->bestMove = NULL_MOVE;
        if (bestScore <= originalAlpha) ttEntry->type = UPPER_BOUND;
        else if (bestScore >= beta) ttEntry->type = LOWER_BOUND;
        else ttEntry->type = EXACT;
    }

    return bestScore;
}

// PVS (Principal variation search)
inline int pvs(int depth, int alpha, int beta, int plyFromRoot, const Move &move, int moveScore, int lmr, bool inCheck, bool pvNode)
{
    // LMR (Late move reduction)
    if (!inCheck && depth >= LMR_MIN_DEPTH && moveScore < KILLER_SCORE)
    {
        lmr -= board.inCheck(); // reduce checks less
        lmr -= pvNode;          // reduce pv nodes less
        if (!board.isCapture(move))
        {
            // This move is a non-killer quiet
            int stm = (int)board.sideToMove();
            int pieceType = (int)board.at<PieceType>(move.from());
            int squareTo = (int)move.to();
            // reduce moves with good history less and vice versa
            lmr -= round(historyMoves[stm][pieceType][squareTo] / LMR_HISTORY_DIVISOR);
        }
        if (lmr < 0) lmr = 0;
    }
    else
        lmr = 0;

    int score = -search(depth - 1 - lmr, -alpha - 1, -alpha, plyFromRoot + 1);
    if (score > alpha && (score < beta || lmr > 0))
        score = -search(depth - 1, -beta, -alpha, plyFromRoot + 1);

    return score;
}

inline int search(int depth, int alpha, int beta, int plyFromRoot, bool skipNmp) // skipNmp defaults to false
{
    if (plyFromRoot > maxPlyReached) maxPlyReached = plyFromRoot;
    if (plyFromRoot > 0 && board.isRepetition(1)) return 0;
    if (checkIsTimeUp()) return 0;

    bool inCheck = board.inCheck();
    if (inCheck) depth++; // Check extension

    int originalAlpha = alpha;
    U64 boardKey = board.zobrist();

    // Probe TT
    TTEntry *ttEntry = &(TT[boardKey % NUM_TT_ENTRIES]);
    if (plyFromRoot > 0 && ttEntry->key == boardKey && ttEntry->depth >= depth)
        if (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha))
        {
            int ret = ttEntry->score;
            if (abs(ret) >= MIN_MATE_SCORE) ret += ret < 0 ? plyFromRoot : -plyFromRoot;
            return ret;
        }

    // IIR (Internal iterative reduction)
    if (ttEntry->key != boardKey && depth >= IIR_MIN_DEPTH && !inCheck)
        depth--;

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) return inCheck ? NEG_INFINITY + plyFromRoot : 0; // checkmate or draw

    if (depth <= 0) return qSearch(alpha, beta, plyFromRoot);

    bool pvNode = beta - alpha > 1 || plyFromRoot == 0;
    int eval = network.Evaluate((int)board.sideToMove());

    if (!pvNode && !inCheck)
    {
        // RFP (Reverse futility pruning)
        if (depth <= RFP_MAX_DEPTH && eval >= beta + RFP_DEPTH_MULTIPLIER * depth)
            return eval;

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && !skipNmp && eval >= beta && board.hasAtLeast1Piece())
        {
            board.makeNullMove();
            int score = -search(depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR, -beta, -alpha, plyFromRoot + 1, true);
            board.unmakeNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    int scores[218];
    scoreMoves(moves, scores, boardKey, *ttEntry, plyFromRoot);
    int bestScore = NEG_INFINITY;
    Move bestMove;

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = incrementalSort(moves, scores, i);
        bool historyMoveOrLosing = scores[i] < KILLER_SCORE;
        int lmr = lmrTable[depth][i];

        // Moves loop pruning
        if (plyFromRoot > 0 && historyMoveOrLosing && bestScore > -MIN_MATE_SCORE)
        {
            // LMP (Late move pruning)
            if (!inCheck && !pvNode && depth <= LMP_MAX_DEPTH && i >= LMP_MIN_MOVES_BASE + depth * depth * 2)
                break;

            // FP (Futility pruning)
            if (!inCheck && depth <= FP_MAX_DEPTH && alpha < MIN_MATE_SCORE && eval + FP_BASE + max(depth - lmr, 0) * FP_MULTIPLIER <= alpha)
                break;

            // SEE pruning
            bool isNoisy = board.isCapture(move) || move.typeOf() == move.PROMOTION;
            if (depth <= SEE_PRUNING_MAX_DEPTH && !SEE(board, move, depth * (isNoisy ? SEE_PRUNING_NOISY_THRESHOLD : SEE_PRUNING_QUIET_THRESHOLD)))
                continue;
        }

        // 7th-rank-pawn extension
        Rank rank = utils::squareRank(move.to());
        int extension = board.at<PieceType>(move.from()) == PieceType::PAWN && (rank == Rank::RANK_2 || rank == Rank::RANK_7);

        board.makeMove(move);
        nodes++;
        int score = i == 0 ? -search(depth - 1 + extension, -beta, -alpha, plyFromRoot + 1) 
                           : pvs(depth + extension, alpha, beta, plyFromRoot, move, scores[i], lmr, inCheck, pvNode);
        board.unmakeMove(move);
        if (checkIsTimeUp()) return 0;

        if (score <= bestScore) continue;
        bestScore = score;
        bestMove = move;

        if (bestScore > alpha) 
        {
            alpha = bestScore;
            if (plyFromRoot == 0) bestMoveRoot = move;
        }

        if (alpha < beta) continue;

        // Beta cutoff

        bool isQuietMove = !board.isCapture(move) && move.typeOf() != move.PROMOTION;
        if (isQuietMove && move != killerMoves[plyFromRoot][0])
        {
            killerMoves[plyFromRoot][1] = killerMoves[plyFromRoot][0];
            killerMoves[plyFromRoot][0] = move;
        }
        if (isQuietMove)
        {
            counterMove = move;
            int stm = (int)board.sideToMove();
            int pieceType = (int)board.at<PieceType>(move.from());
            int squareTo = (int)move.to();
            historyMoves[stm][pieceType][squareTo] += depth * depth;
        }

        break; // Beta cutoff
    }

    // Save in TT
    ttEntry->key = boardKey;
    ttEntry->depth = depth;
    ttEntry->score = bestScore;
    if (abs(bestScore) >= MIN_MATE_SCORE) ttEntry->score += bestScore < 0 ? -plyFromRoot : plyFromRoot;
    ttEntry->bestMove = bestMove;
    if (bestScore <= originalAlpha) ttEntry->type = UPPER_BOUND;
    else if (bestScore >= beta) ttEntry->type = LOWER_BOUND;
    else ttEntry->type = EXACT;

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

        if (checkIsTimeUp()) return 0;

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
    // clear history moves
    memset(&historyMoves[0][0][0], 0, sizeof(historyMoves));

    counterMove = NULL_MOVE;
    nodes = 0;
    int iterationDepth = 1, score = 0;

    while (iterationDepth <= MAX_DEPTH)
    {
        maxPlyReached = -1;
        int scoreBefore = score;
        Move moveBefore = bestMoveRoot;

        score = iterationDepth >= ASPIRATION_MIN_DEPTH ? aspiration(iterationDepth, score) 
                                                       : search(iterationDepth, NEG_INFINITY, POS_INFINITY, 0);

        if (checkIsTimeUp())
        {
            bestMoveRoot = moveBefore;
            info(iterationDepth, scoreBefore);
            break;
        }

        info(iterationDepth, score);
        iterationDepth++;
    }

    return bestMoveRoot;
}

#endif