#pragma once

// clang-format off

#include "tunable_params.hpp"
#include "time_manager.hpp"
#include "tt.hpp"
#include "history_entry.hpp"
#include "see.hpp"
#include "nnue.hpp"

namespace uci 
{ 
inline void info(int depth, i16 score); 
}

namespace search {

const u8 MAX_DEPTH = 100;

// Move ordering
const i32 TT_MOVE_SCORE           = I32_MAX,
          GOOD_NOISY_BASE_SCORE   = 1'500'000'000,
          KILLER_SCORE            = 1'000'000'000,
          COUNTERMOVE_SCORE       = 500'000'000,
          HISTORY_MOVE_BASE_SCORE = 0;
i32       BAD_NOISY_BASE_SCORE    = -historyMax.value / 2;

// Most valuable victim    P    N    B    R    Q    K  NONE
const i32 MVV_VALUES[7] = {100, 300, 320, 500, 900, 0, 0};

u8 maxDepth;
TimeManager timeManager;
u64 nodes;
int maxPlyReached;
u64 movesNodes[1ULL << 16];             // [moveEncoded]
Move pvLines[MAX_DEPTH+1][MAX_DEPTH+1]; // [ply][ply]
int pvLengths[MAX_DEPTH+1];             // [pvLineIndex]
int lmrTable[MAX_DEPTH+1][256];         // [depth][moveIndex]
Move killerMoves[MAX_DEPTH];            // [ply]
Move countermoves[2][1ULL << 16];       // [color][moveEncoded]
HistoryEntry historyTable[2][6][64];    // [color][pieceType][targetSquare]

namespace internal
{
inline i16 iterativeDeepening();
}

inline void init()
{
    tt::resize(tt::DEFAULT_SIZE_MB);

    // init lmrTable
    for (int depth = 0; depth < MAX_DEPTH+1; depth++)
        for (int move = 0; move < 256; move++)
            lmrTable[depth][move] = depth == 0 || move == 0 
                                    ? 0 : round(lmrBase.value + ln(depth) * ln(move) * lmrMultiplier.value);
}

inline std::pair<Move, i16> search(TimeManager _timeManager, u8 _maxDepth = MAX_DEPTH)
{
    // reset and initialize stuff
    nodes = 0;
    memset(movesNodes, 0, sizeof(movesNodes));
    memset(pvLines, 0, sizeof(pvLines));
    memset(pvLengths, 0, sizeof(pvLengths));
    maxDepth = min(_maxDepth, MAX_DEPTH);
    timeManager = _timeManager;

    i16 score = internal::iterativeDeepening();

    if (tt::age < 63) tt::age++;

    // return best move and score
    return { pvLines[0][0], score };
}

inline std::pair<Move, i16> search(u8 _maxDepth = MAX_DEPTH) {
    return search(TimeManager(), _maxDepth);
}

namespace internal {

inline i16 aspiration(u8 iterationDepth, i16 score);

inline i16 search(i16 depth, u16 ply, i16 alpha, i16 beta, bool cutNode, 
                  i8 doubleExtensionsLeft, bool singular = false, i16 eval = 0);

inline i16 qSearch(int ply, i16 alpha, i16 beta);

inline std::array<i32, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove);

inline i16 iterativeDeepening()
{
    i16 score = 0, lastScore = 0;

    for (u16 iterationDepth = 1; iterationDepth <= maxDepth; iterationDepth++)
    {
        maxPlyReached = -1;

        score = iterationDepth >= aspMinDepth.value
                ? aspiration(iterationDepth, score) 
                : search(iterationDepth, 0, NEG_INFINITY, POS_INFINITY, false, maxDoubleExtensions.value);

        if (timeManager.isHardTimeUp(nodes)) return lastScore;

        uci::info(iterationDepth, score);

        u64 bestMoveNodes = movesNodes[pvLines[0][0].getMoveEncoded()];
        if (timeManager.isSoftTimeUp(nodes, bestMoveNodes)) break;

        lastScore = score;
    }

    return score;
}

inline i16 aspiration(u8 iterationDepth, i16 score)
{
    // Aspiration Windows
    // Search with a small window, adjusting it and researching until the score is inside the window

    i16 delta = aspInitialDelta.value;
    i16 alpha = max(NEG_INFINITY, score - delta);
    i16 beta = min(POS_INFINITY, score + delta);
    i16 depth = iterationDepth;

    while (true)
    {
        score = search(depth, 0, alpha, beta, false, maxDoubleExtensions.value);

        if (timeManager.isHardTimeUp(nodes)) return 0;

        if (score >= beta)
        {
            beta = min(beta + delta, POS_INFINITY);
            depth--;
        }
        else if (score <= alpha)
        {
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, NEG_INFINITY);
            depth = iterationDepth;
        }
        else
            break;

        delta *= aspDeltaMultiplier.value;
    }

    return score;
}

inline i16 evaluate() {
    return std::clamp(nnue::evaluate(board.sideToMove()), -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

inline i16 search(i16 depth, u16 ply, i16 alpha, i16 beta, bool cutNode, 
                  i8 doubleExtensionsLeft, bool singular, i16 eval)
{
    if (timeManager.isHardTimeUp(nodes)) return 0;

    pvLengths[ply] = 0; // Ensure fresh PV

    // Drop into qsearch on terminal nodes
    if (depth <= 0) return qSearch(ply, alpha, beta);

    // Update seldepth
    if (ply > maxPlyReached) maxPlyReached = ply;

    if (ply > 0 && board.isDraw()) return 0;

    if (ply >= maxDepth) 
        return board.inCheck() ? 0 : evaluate();

    if (depth > maxDepth) depth = maxDepth;

    // Probe TT
    auto [ttEntry, shouldCutoff] = tt::probe(board.getZobristHash(), depth, ply, alpha, beta);
    if (shouldCutoff && !singular) 
        return ttEntry->adjustedScore(ply);

    bool ttHit = board.getZobristHash() == ttEntry->zobristHash;
    Move ttMove = ttHit ? ttEntry->bestMove : MOVE_NONE;

    bool pvNode = beta - alpha > 1 || ply == 0;
    if (cutNode) assert(!pvNode); // cutNode implies !pvNode

    // We don't use eval in check because it's unreliable, so don't bother calculating it if in check
    // In singular search we already have the eval, passed in the eval arg
    if (!board.inCheck() && !singular)
        eval = evaluate();

    if (!pvNode && !board.inCheck() && !singular)
    {
        // RFP (Reverse futility pruning) / Static NMP
        if (depth <= rfpMaxDepth.value && eval >= beta + depth * rfpDepthMultiplier.value)
            return eval;

        // AP (Alpha pruning)
        if (depth <= apMaxDepth.value && eval + apMargin.value <= alpha)
            return eval; 

        // Razoring
        if (depth <= razoringMaxDepth.value && alpha > eval + depth * razoringDepthMultiplier.value) {
            i16 score = qSearch(ply, alpha, beta);
            if (score <= alpha) return score;
        }

        // NMP (Null move pruning)
        if (depth >= nmpMinDepth.value && board.getLastMove() != MOVE_NONE
        && board.hasNonPawnMaterial(board.sideToMove()) && eval >= beta)
        {
            board.makeNullMove();
            int nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value - min((eval - beta)/200, 3);
            i16 score = -search(nmpDepth, ply + 1, -beta, -alpha, !cutNode, doubleExtensionsLeft);
            board.undoNullMove();

            if (score >= MIN_MATE_SCORE) return beta;
            if (score >= beta) return score;
        }
    }

    bool trySingular = !singular && depth >= singularMinDepth.value
                       && abs(ttEntry->score) < MIN_MATE_SCORE
                       && ttEntry->depth >= depth - singularDepthMargin.value
                       && ttEntry->getBound() != tt::UPPER_BOUND;
                       
    // IIR (Internal iterative reduction)
    if (!ttHit && depth >= iirMinDepth.value && !board.inCheck())
        depth--;

    // generate all moves except underpromotions
    MovesList moves = board.pseudolegalMoves(false, false); 
    auto movesScores = scoreMoves(moves, ttMove, killerMoves[ply]);

    int stm = (int)board.sideToMove();
    int legalMovesPlayed = 0;
    i16 bestScore = NEG_INFINITY;
    Move bestMove = MOVE_NONE;
    i16 originalAlpha = alpha;

    // Fail low quiets at beginning of array, fail low noisy moves at the end
    HistoryEntry *failLowsHistoryEntry[256]; 
    int numFailLowQuiets = 0, numFailLowNoisies = 0;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // Don't search TT move in singular search
        if (singular && move == ttMove) continue;

        bool isQuietMove = !board.isCapture(move) && move.promotion() == PieceType::NONE;
        int lmr = lmrTable[depth][legalMovesPlayed + 1];

        // Moves loop pruning
        if (ply > 0 && moveScore < COUNTERMOVE_SCORE && bestScore > -MIN_MATE_SCORE)
        {
            // LMP (Late move pruning)
            if (depth <= lmpMaxDepth.value
            && legalMovesPlayed >= lmpMinMoves.value + pvNode + board.inCheck() + depth * depth * lmpDepthMultiplier.value)
                break;

            // FP (Futility pruning)
            if (depth <= fpMaxDepth.value && !board.inCheck() && alpha < MIN_MATE_SCORE 
            && eval + fpBase.value + max(depth - lmr, 0) * fpMultiplier.value <= alpha)
                break;

            // SEE pruning
            int threshold = isQuietMove ? depth * seeQuietThreshold.value : depth * depth * seeNoisyThreshold.value;
            if (depth <= seePruningMaxDepth.value && !see::SEE(board, move, threshold))
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
        if (trySingular && move == ttMove)
        {
            // Singular search: before searching any move, search this node at a shallower depth with TT move excluded

            board.undoMove(); // undo TT move we just made

            i16 singularBeta = max(NEG_INFINITY, ttEntry->score - depth * singularBetaMultiplier.value);
            i16 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                       cutNode, doubleExtensionsLeft, true, eval);

            board.makeMove(move, false); // second arg = false => don't check legality (we already verified it's a legal move)

            // Double extension
            if (!pvNode && doubleExtensionsLeft > 0 && singularScore < singularBeta - singularBetaMargin.value)
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
            // Cutnode negative extension
            else if (cutNode)
                extension = -1;
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
        HistoryEntry *historyEntry = &(historyTable[stm][pieceType][targetSquare]);

        // LMR (Late move reductions)
        if (legalMovesPlayed > 1 && depth >= 3 && moveScore <= KILLER_SCORE)
        {
            lmr -= board.inCheck(); // reduce checks less
            lmr -= pvNode; // reduce pv nodes less

            // reduce killers and countermoves less
            if (moveScore == KILLER_SCORE || moveScore == COUNTERMOVE_SCORE)
                lmr--;
            // reduce moves with good history less and vice versa
            else if (isQuietMove)
                lmr -= round((moveScore - HISTORY_MOVE_BASE_SCORE) / (double)lmrHistoryDivisor.value);
            else 
                lmr -= round(historyEntry->noisyHistory / (double)lmrNoisyHistoryDivisor.value);

            // if lmr is negative, we would have an extension instead of a reduction
            // dont reduce into qsearch
            lmr = std::clamp(lmr, 0, searchDepth - 1);

            // PVS part 1/4: reduced search on null window
            score = -search(searchDepth - lmr, ply + 1, -alpha-1, -alpha, true, doubleExtensionsLeft);

            // PVS part 2/4: if score is better than expected (score > alpha), do full depth search on null window
            if (score > alpha && lmr != 1)
                score = -search(searchDepth, ply + 1, -alpha-1, -alpha, !cutNode, doubleExtensionsLeft);
        }
        else if (!pvNode || legalMovesPlayed > 1)
            // PVS part 3/4: full depth search on null window
            score = -search(searchDepth, ply + 1, -alpha-1, -alpha, !cutNode, doubleExtensionsLeft);

        // PVS part 4/4: full depth search on full window for some pv nodes
        if (pvNode && (legalMovesPlayed == 1 || score > alpha))
            score = -search(searchDepth, ply + 1, -beta, -alpha, false, doubleExtensionsLeft);

        board.undoMove();
        if (timeManager.isHardTimeUp(nodes)) return 0;

        if (ply == 0) movesNodes[move.getMoveEncoded()] += nodes - prevNodes;

        if (score > bestScore) bestScore = score;

        if (score <= alpha) // Fail low
        {
            // Fail low quiets at beginning of array, fail low noisy moves at the end
            if (isQuietMove)
                failLowsHistoryEntry[numFailLowQuiets++] = historyEntry;
            else
                failLowsHistoryEntry[256 - ++numFailLowNoisies] = historyEntry;

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
            // memcpy(dst, src, size)
            memcpy(&(pvLines[ply][1]), pvLines[ply + 1], subPvLineLength * sizeof(Move));
        }

        if (score < beta) continue;

        // Fail high / Beta cutoff

        i32 historyBonus = min(historyMinBonus.value, historyBonusMultiplier.value * (depth-1));

        if (isQuietMove)
        {
            // This quiet move is a killer move and a countermove
            killerMoves[ply] = move;
            if (board.getLastMove() != MOVE_NONE)
                countermoves[stm][board.getLastMove().getMoveEncoded()] = move;

            // Increase this quiet's history
            historyEntry->updateQuietHistory(board, historyBonus);

            // Penalize/decrease history of quiets that failed low
            for (int i = 0; i < numFailLowQuiets; i++)
                failLowsHistoryEntry[i]->updateQuietHistory(board, -historyBonus); 
        }
        else
        {
            // Increase history of this noisy move
            historyEntry->updateNoisyHistory(board, historyBonus);

            // Penalize/decrease history of noisy moves that failed low
            for (int i = 255, j = 0; j < numFailLowNoisies; i--, j++)
                failLowsHistoryEntry[i]->updateNoisyHistory(board, -historyBonus);
        }

        break; // Fail high / Beta cutoff
    }

    if (legalMovesPlayed == 0) 
        // checkmate or stalemate
        return board.inCheck() ? NEG_INFINITY + ply : 0;

    if (!singular)
        tt::store(ttEntry, board.getZobristHash(), depth, bestScore, bestMove, ply, originalAlpha, beta);    

    return bestScore;
}

inline i16 qSearch(int ply, i16 alpha, i16 beta)
{
    // Quiescence search: search noisy moves until a 'quiet' position is reached

    // Update seldepth
    if (ply > maxPlyReached) maxPlyReached = ply;

    if (board.isDraw()) return 0;

    if (ply >= maxDepth) 
        return board.inCheck() ? 0 : evaluate();

    i16 eval = NEG_INFINITY; // eval is NEG_INFINITY in check
    if (!board.inCheck())
    {
        eval = evaluate();
        if (eval >= beta) return eval;
        if (eval > alpha) alpha = eval;
    }

    // Probe TT
    auto [ttEntry, shouldCutoff] = tt::probe(board.getZobristHash(), 0, ply, alpha, beta);
    if (shouldCutoff) return ttEntry->adjustedScore(ply);

    Move ttMove = board.getZobristHash() == ttEntry->zobristHash ? ttEntry->bestMove : MOVE_NONE;

    // if in check, generate all moves, else only noisy moves
    // never generate underpromotions
    MovesList moves = board.pseudolegalMoves(!board.inCheck(), false); 
    auto movesScores = scoreMoves(moves, ttMove, killerMoves[ply]);
    
    int legalMovesPlayed = 0;
    i16 bestScore = eval;
    Move bestMove = MOVE_NONE;
    i16 originalAlpha = alpha;

    for (int i = 0; i < moves.size(); i++)
    {
        auto [move, moveScore] = incrementalSort(moves, movesScores, i);

        // SEE pruning (skip bad noisy moves)
        if (!board.inCheck() && moveScore < BAD_NOISY_BASE_SCORE + 100'000) 
            continue;

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

    tt::store(ttEntry, board.getZobristHash(), 0, bestScore, bestMove, ply, originalAlpha, beta);    

    return bestScore;
}

inline std::array<i32, 256> scoreMoves(MovesList &moves, Move ttMove, Move killerMove)
{
    std::array<i32, 256> movesScores;

    int stm = (int)board.sideToMove();
    Move countermove = board.getLastMove() == MOVE_NONE
                       ? MOVE_NONE
                       : countermoves[stm][board.getLastMove().getMoveEncoded()];

    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];

        if (move == ttMove)
        {
            movesScores[i] = TT_MOVE_SCORE;
            continue;
        }

        PieceType captured = board.captured(move);
        PieceType promotion = move.promotion();
        int pieceType = (int)board.pieceTypeAt(move.from());
        int targetSquare = (int)move.to();
        HistoryEntry *historyEntry = &(historyTable[stm][pieceType][targetSquare]);

        if (captured != PieceType::NONE || promotion != PieceType::NONE)
        {
            movesScores[i] = see::SEE(board, move) ? GOOD_NOISY_BASE_SCORE : BAD_NOISY_BASE_SCORE;
            movesScores[i] += MVV_VALUES[(int)captured];
            movesScores[i] += historyEntry->noisyHistory;
        }
        else if (move == killerMove)
            movesScores[i] = KILLER_SCORE;
        else if (move == countermove)
            movesScores[i] = COUNTERMOVE_SCORE;
        else
        {
            movesScores[i] = HISTORY_MOVE_BASE_SCORE;
            movesScores[i] += historyEntry->quietHistory(board);
        }
    }

    return movesScores;
}

}

}

