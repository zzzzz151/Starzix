#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "chess.hpp"
#include "nnue.hpp"
#include "see.hpp"
inline void sendInfo(int depth, int score); // from uci.hpp
using namespace chess;
using namespace std;

// ----- Tunable params ------

const uint32_t DEFAULT_TIME_MILLISECONDS = 60000; // default 1 minute

const uint8_t MAX_DEPTH = 50;

int TT_SIZE_MB = 64; // default (and current) TT size in MB

extern const int PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0},
                 SEE_PIECE_VALUES[7] = {100, 300, 300, 500, 900, 0, 0},
                 PAWN_INDEX = 0;

const int HASH_MOVE_SCORE = INT_MAX,
          GOOD_CAPTURE_SCORE = 1'500'000'000,
          PROMOTION_SCORE = 1'000'000'000,
          KILLER_SCORE = 500'000'000,
          HISTORY_SCORE = 0,
          BAD_CAPTURE_SCORE = -500'000'000;

const uint8_t IIR_MIN_DEPTH = 4;

const uint8_t RFP_MIN_DEPTH = 8,
              RFP_DEPTH_MULTIPLIER = 75;

const uint8_t NMP_MIN_DEPTH = 3,
              NMP_BASE_REDUCTION = 3,
              NMP_REDUCTION_DIVISOR = 3;

const uint8_t FP_MAX_DEPTH = 7,
              FP_BASE = 160,
              FP_LMR_MULTIPLIER = 65;

const uint8_t LMR_MIN_DEPTH = 2;
const double LMR_BASE = 0.77,
             LMR_DIVISOR = 2.36;

const uint8_t ASPIRATION_MIN_DEPTH = 6,
              ASPIRATION_INITIAL_DELTA = 25;
const double ASPIRATION_DELTA_MULTIPLIER = 1.5;

// ----- End tunable params -----

const int POS_INFINITY = 9999999, NEG_INFINITY = -POS_INFINITY, MIN_MATE_SCORE = POS_INFINITY - 512;
Move NULL_MOVE;
U64 nodes;
chrono::steady_clock::time_point start;
int millisecondsForThisTurn;
bool isTimeUp = false;
Board board;
Move bestMoveRoot;
Move killerMoves[512][2]; // ply, move
int killerMovesNumRows = 512;
int historyMoves[2][6][64];       // color, pieceType, squareTo
int lmrTable[MAX_DEPTH + 2][218]; // depth, move (lmrTable initialized in main.cpp)
int lmrTableNumRows = MAX_DEPTH + 2, lmrTableNumCols = 218;

const char EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    U64 key = 0;
    int score;
    Move bestMove;
    int16_t depth;
    char type;
};
uint32_t NUM_TT_ENTRIES;
vector<TTEntry> TT;

inline bool checkIsTimeUp()
{
    if (isTimeUp) return true;
    isTimeUp = (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= millisecondsForThisTurn;
    return isTimeUp;
}

inline void scoreMoves(Movelist &moves, int *scores, U64 boardKey, TTEntry &ttEntry, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (ttEntry.key == boardKey && move == ttEntry.bestMove)
            scores[i] = HASH_MOVE_SCORE;
        else if (board.isCapture(move))
        {
            PieceType captured = board.at<PieceType>(move.to());
            PieceType capturing = board.at<PieceType>(move.from());
            int moveScore = SEE(board, move) ? GOOD_CAPTURE_SCORE : BAD_CAPTURE_SCORE;
            moveScore += 100 * PIECE_VALUES[(int)captured] - PIECE_VALUES[(int)capturing]; // MVVLVA
            scores[i] = moveScore;
        }
        else if (move.typeOf() == move.PROMOTION)
            scores[i] = PROMOTION_SCORE;
        else if (killerMoves[plyFromRoot][0] == move || killerMoves[plyFromRoot][1] == move)
            scores[i] = KILLER_SCORE;
        else
        {
            int stm = (int)board.sideToMove();
            int pieceType = (int)board.at<PieceType>(move.from());
            int squareTo = (int)move.to();
            scores[i] = HISTORY_SCORE + historyMoves[stm][pieceType][squareTo];
        }
    }
}

inline int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence saarch: search capture moves until a 'quiet' position is reached

    int eval = network.Evaluate((int)board.sideToMove());
    if (eval >= beta) return eval;
    if (alpha < eval) alpha = eval;

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

        if (score >= beta) return score; // in CPW: return beta;
        if (score > alpha) alpha = score;
    }

    return bestScore;
}

inline int search(int depth, int plyFromRoot, int alpha, int beta, bool doNull = true)
{
    if (plyFromRoot > 0 && board.isRepetition()) return 0;

    if (checkIsTimeUp()) return 0;

    if (depth < 0) depth = 0;

    int originalAlpha = alpha;

    bool inCheck = board.inCheck();
    if (inCheck) depth++;

    U64 boardKey = board.zobrist();
    TTEntry *ttEntry = &(TT[boardKey % NUM_TT_ENTRIES]);
    if (plyFromRoot > 0 && ttEntry->key == boardKey && ttEntry->depth >= depth)
        if (ttEntry->type == EXACT || (ttEntry->type == LOWER_BOUND && ttEntry->score >= beta) || (ttEntry->type == UPPER_BOUND && ttEntry->score <= alpha))
            return ttEntry->score;

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
        if (depth <= RFP_MIN_DEPTH && eval >= beta + RFP_DEPTH_MULTIPLIER * depth)
            return eval;

        // NMP (Null move pruning)
        if (depth >= NMP_MIN_DEPTH && doNull && eval >= beta)
        {
            bool hasAtLeast1Piece =
                board.pieces(PieceType::KNIGHT, board.sideToMove()) > 0 ||
                board.pieces(PieceType::BISHOP, board.sideToMove()) > 0 ||
                board.pieces(PieceType::ROOK, board.sideToMove()) > 0 ||
                board.pieces(PieceType::QUEEN, board.sideToMove()) > 0;

            if (hasAtLeast1Piece)
            {
                board.makeNullMove();
                int score = -search(depth - NMP_BASE_REDUCTION - depth / NMP_REDUCTION_DIVISOR, plyFromRoot + 1, -beta, -alpha, false);
                board.unmakeNullMove();
                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }
        }
    }

    int scores[218];
    scoreMoves(moves, scores, boardKey, *ttEntry, plyFromRoot);
    int bestScore = NEG_INFINITY;
    Move bestMove;
    bool canLmr = depth >= LMR_MIN_DEPTH && !inCheck;

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

        bool quietOrLosing = scores[i] < KILLER_SCORE;
        int lmr = lmrTable[depth][i];

        // FP (Futility pruning)
        if (plyFromRoot > 0 && depth <= FP_MAX_DEPTH && !inCheck && quietOrLosing && bestScore > -MIN_MATE_SCORE && alpha < MIN_MATE_SCORE)
            if (eval + FP_BASE + max(depth - lmr, 0) * FP_LMR_MULTIPLIER <= alpha)
                continue;

        board.makeMove(move);
        nodes++;

        // PVS (Principal variation search)
        int score = 0;
        if (i == 0)
            score = -search(depth - 1, plyFromRoot + 1, -beta, -alpha);
        else
        {
            // LMR (Late move reduction)
            if (canLmr)
            {
                lmr -= board.inCheck(); // reduce checks less
                lmr -= pvNode;          // reduce pv nodes less
                if (lmr < 0) lmr = 0;
            }
            else
                lmr = 0;

            score = -search(depth - 1 - lmr, plyFromRoot + 1, -alpha - 1, -alpha);
            if (score > alpha && (score < beta || lmr > 0))
                score = -search(depth - 1, plyFromRoot + 1, -beta, -alpha);
        }

        board.unmakeMove(move);
        if (checkIsTimeUp()) return 0;

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;
            if (plyFromRoot == 0) bestMoveRoot = move;

            if (bestScore > alpha) alpha = bestScore;
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
    ttEntry->score = bestScore;
    ttEntry->bestMove = bestMove;
    if (bestScore <= originalAlpha)
        ttEntry->type = UPPER_BOUND;
    else if (bestScore >= beta)
        ttEntry->type = LOWER_BOUND;
    else
        ttEntry->type = EXACT;

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
        score = search(depth, 0, alpha, beta);

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

const byte TIME_TYPE_NORMAL_GAME = (byte)0, TIME_TYPE_MOVE_TIME = (byte)1;

inline Move iterativeDeepening(int milliseconds, byte timeType, bool info = false)
{
    bestMoveRoot = NULL_MOVE;
    nodes = 0;
    isTimeUp = false;

    if (timeType == TIME_TYPE_NORMAL_GAME)
        millisecondsForThisTurn = milliseconds / 30.0;
    else if (timeType == TIME_TYPE_MOVE_TIME)
        millisecondsForThisTurn = milliseconds;
    else
        millisecondsForThisTurn = DEFAULT_TIME_MILLISECONDS;

    int iterationDepth = 1, score = 0;
    while (iterationDepth <= MAX_DEPTH)
    {
        int scoreBefore = score;
        Move moveBefore = bestMoveRoot;

        if (iterationDepth < ASPIRATION_MIN_DEPTH)
            score = search(iterationDepth, 0, NEG_INFINITY, POS_INFINITY);
        else
            score = aspiration(iterationDepth, score);

        if (checkIsTimeUp())
        {
            bestMoveRoot = moveBefore;
            sendInfo(iterationDepth - 1, scoreBefore);
            break;
        }

        if (info) sendInfo(iterationDepth, score);

        iterationDepth++;
    }

    return bestMoveRoot;
}

#endif