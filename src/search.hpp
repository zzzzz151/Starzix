#include "search_params.hpp"
#include "tt.hpp"
#include "see.hpp"
#include "history_entry.hpp"

u8 LMR_TABLE[256][256]; // [depth][moveIndex]

inline void initLmrTable()
{
    memset(LMR_TABLE, 0, sizeof(LMR_TABLE));
    for (int depth = 1; depth < 256; depth++)
        for (int move = 1; move < 256; move++)
            LMR_TABLE[depth][move] = round(lmrBase.value + ln(depth) * ln(move) * lmrMultiplier.value);
}

class Searcher {
    public:

    Board board;
    u8 maxDepth;
    u64 nodes;
    u8 maxPlyReached;
    TT tt;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    bool hardTimeUp;
    u64 softMilliseconds, hardMilliseconds;
    u64 softNodes, hardNodes;
    u64 movesNodes[1ULL << 16];          // [moveEncoded]
    Move pvLines[256][256];              // [ply][ply]
    u8 pvLengths[256];                   // [pvLineIndex]
    Move killerMoves[256];               // [ply]
    Move countermoves[2][6][64];         // [color][pieceType][targetSquare]
    HistoryEntry historyTable[2][6][64]; // [color][pieceType][targetSquare]
    const u16 OVERHEAD_MILLISECONDS = 10;

    inline Searcher(Board board)
    {
        resetLimits();
        this->board = board;
        nodes = 0;
        maxPlyReached = 0;
        tt = TT(true);
        memset(movesNodes, 0, sizeof(movesNodes));
        memset(pvLines, 0, sizeof(pvLines));
        memset(pvLengths, 0, sizeof(pvLengths));
        memset(killerMoves, 0, sizeof(killerMoves));
        memset(countermoves, 0, sizeof(countermoves));
        memset(historyTable, 0, sizeof(historyTable));
    }

    inline Move bestMoveRoot() {
        return pvLines[0][0];
    }

    inline void resetLimits()
    {
        startTime = std::chrono::steady_clock::now();
        maxDepth = 100;
        hardTimeUp = false;
        hardMilliseconds = softMilliseconds
                         = softNodes
                         = hardNodes
                         = U64_MAX;
    }

    inline void setTimeLimits(u64 milliseconds, u64 incrementMilliseconds, u64 movesToGo, bool isMoveTime)
    {
        assert(milliseconds > 0 && movesToGo > 0);
        u64 maxHardMs = max((i64)1, (i64)milliseconds - (i64)OVERHEAD_MILLISECONDS);

        if (isMoveTime)
        {
            softMilliseconds = U64_MAX;
            hardMilliseconds = std::clamp(milliseconds, (u64)1, maxHardMs);
            return;
        }

        u64 alloc = min((u64)(milliseconds / movesToGo + incrementMilliseconds * 2.0/3.0), milliseconds);
        hardMilliseconds = std::clamp(alloc * 2, (u64)1, maxHardMs);
        softMilliseconds = movesToGo == 1 ? alloc : alloc * softMillisecondsPercentage.value;
    }

    inline bool isHardTimeUp()
    {
        if (hardTimeUp || nodes >= hardNodes) return true;

        // Check time every 1024 nodes
        if ((nodes % 1024) != 0) return false;

        return hardTimeUp = (millisecondsElapsed(startTime) >= hardMilliseconds);
    }

    inline double bestMoveNodesFraction() 
    {
        assert(bestMoveRoot() != MOVE_NONE);
        return (double)movesNodes[bestMoveRoot().getMoveEncoded()] / (double)nodes;
    }

    inline std::pair<Move, i32> search(bool printInfo = true)
    {
        // reset and initialize stuff
        //startTime = std::chrono::steady_clock::now();
        hardTimeUp = false;
        nodes = 0;
        maxPlyReached = 0;
        memset(movesNodes, 0, sizeof(movesNodes));
        memset(pvLines, 0, sizeof(pvLines));
        memset(pvLengths, 0, sizeof(pvLengths));

        // ID (Iterative deepening)
        i32 score = 0;
        for (i32 iterationDepth = 1; iterationDepth <= maxDepth; iterationDepth++)
        {
            maxPlyReached = 0;
            i32 iterationScore = iterationDepth >= aspMinDepth.value 
                                 ? aspiration(iterationDepth, score)
                                 : search(iterationDepth, 0, -INF, INF, doubleExtensionsMax.value);

            if (isHardTimeUp()) break;

            score = iterationScore;
            u64 msElapsed = millisecondsElapsed(startTime);

            if (printInfo)
            {
                std::cout << "info depth " << iterationDepth
                          << " seldepth " << (int)maxPlyReached
                          << " time " << msElapsed
                          << " nodes " << nodes
                          << " nps " << nodes * 1000 / max(msElapsed, (u64)1);
                if (abs(score) < MIN_MATE_SCORE)
                    std::cout << " score cp " << score;
                else
                {
                    i32 pliesToMate = INF - abs(score);
                    i32 movesTillMate = round(pliesToMate / 2.0);
                    std::cout << " score mate " << (score > 0 ? movesTillMate : -movesTillMate);
                }
                std::cout << " pv " << pvLines[0][0].toUci();
                for (int i = 1; i < pvLengths[0]; i++)
                    std::cout << " " << pvLines[0][i].toUci();
                std::cout << std::endl;
            }

            // Check soft limits
            if (nodes >= softNodes)
                break;
            if (msElapsed >= (iterationDepth >= softTimeMoveNodesScalingMinDepth.value
                              ? softMilliseconds 
                                * (softTimeMoveNodesScalingBase.value - bestMoveNodesFraction()) 
                                * softTimeMoveNodesScalingMultiplier.value
                              : softMilliseconds))
                break;
        }

        return { bestMoveRoot(), score };
    }

    private:

    inline i32 aspiration(i32 iterationDepth, i32 score)
    {
        // Aspiration Windows
        // Search with a small window, adjusting it and researching until the score is inside the window

        i32 delta = aspInitialDelta.value;
        i32 alpha = max(-INF, score - delta);
        i32 beta = min(INF, score + delta);
        i32 depth = iterationDepth;

        while (true)
        {
            score = search(depth, 0, alpha, beta, doubleExtensionsMax.value);

            if (isHardTimeUp()) return 0;

            if (score >= beta)
            {
                beta = min(beta + delta, INF);
                depth--;
            }
            else if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = max(alpha - delta, -INF);
                depth = iterationDepth;
            }
            else
                break;

            delta *= aspDeltaMultiplier.value;
        }

        return score;
    }

    inline i32 search(i32 depth, u8 ply, i32 alpha, i32 beta, 
                      u8 doubleExtensionsLeft, bool singular = false, i32 eval = EVAL_NONE)
    { 
        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (ply > 0 && !singular && board.isDraw()) return 0;

        if (ply >= maxDepth) return board.inCheck() ? 0 : board.evaluate();

        if (depth > maxDepth) depth = maxDepth;

        TTEntry *ttEntry = tt.probe(board.zobristHash());
        if (tt.cutoff(ttEntry, board.zobristHash(), depth, ply, alpha, beta) && !singular) 
            return ttEntry->adjustedScore(ply);

        Color stm = board.sideToMove();
        bool pvNode = beta > alpha + 1;
        if (eval == EVAL_NONE)
            eval = board.inCheck() ? EVAL_NONE : board.evaluate();

        if (!pvNode && !board.inCheck() && !singular)
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth.value && eval >= beta + depth * rfpDepthMultiplier.value)
                return eval;

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth.value && eval >= beta
            && board.lastMove() != MOVE_NONE && board.hasNonPawnMaterial(stm))
            {
                board.makeMove(MOVE_NONE);

                i32 nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value
                               - min((eval-beta) / nmpEvalBetaDivisor.value, nmpEvalBetaMax.value);
                i32 score = -search(nmpDepth, ply + 1, -beta, -alpha, doubleExtensionsLeft);

                board.undoMove();

                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }
        }

        Move ttMove = board.zobristHash() == ttEntry->zobristHash
                      ? ttEntry->bestMove : MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth.value && ttMove == MOVE_NONE)
            depth--;

        // genenerate all moves except underpromotions
        MovesList moves = MovesList();
        board.pseudolegalMoves(moves, false, false); 
        std::array<i32, 256> movesScores;
        scoreMoves(moves, movesScores, ttMove, killerMoves[ply]);

        u8 legalMovesPlayed = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        i32 originalAlpha = alpha;
        std::array<HistoryEntry*, 256> failLowsHistoryEntry;
        u8 failLowQuiets = 0, failLowNoisies = 0;

        for (int i = 0; i < moves.size(); i++)
        {
            auto [move, moveScore] = incrementalSort(moves, movesScores, i);

            // Don't search TT move in singular search
            if (move == ttMove && singular) continue;

            bool isQuiet = !board.isCapture(move) && move.promotion() == PieceType::NONE;

            if (bestScore > -MIN_MATE_SCORE && !pvNode && !board.inCheck() && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (depth <= lmpMaxDepth.value
                && legalMovesPlayed >= lmpMinMoves.value + depth * depth * lmpDepthMultiplier.value)
                    break;

                i32 lmrDepth = max(0, depth - LMR_TABLE[depth][legalMovesPlayed+1]);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth.value && alpha < MIN_MATE_SCORE
                && alpha > eval + fpBase.value + lmrDepth * fpMultiplier.value)
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth.value 
                && !SEE(board, move, 
                        depth * (isQuiet ? seeQuietThreshold.value : depth * seeNoisyThreshold.value)))
                    continue;
            }

            // skip illegal moves
            if (!board.makeMove(move)) continue;

            i32 extension = 0;
            if (ply == 0) goto skipExtensions;

            // SE (Singular extensions)
            if (move == ttMove
            && depth >= singularMinDepth.value
            && abs(ttEntry->score) < MIN_MATE_SCORE
            && ttEntry->depth >= depth - singularDepthMargin.value
            && ttEntry->getBound() != Bound::UPPER)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                // Undo TT move we just made
                BoardState boardState = board.getBoardState();
                Accumulator accumulator = board.getAccumulator();
                board.undoMove();

                i32 singularBeta = max(-INF, ttEntry->score - depth * singularBetaMultiplier.value);
                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           doubleExtensionsLeft, true, eval);

                // Make the TT move again
                board.pushBoardState(boardState);
                board.pushAccumulator(accumulator);

                // Double extension
                if (singularScore < singularBeta - doubleExtensionMargin.value 
                && !pvNode && doubleExtensionsLeft > 0)
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

            skipExtensions:

            legalMovesPlayed++;
            u64 nodesBefore = nodes;
            nodes++;
            HistoryEntry *historyEntry = &historyTable[(i8)stm][(u8)move.pieceType()][move.to()];

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;
            if (legalMovesPlayed == 1)
            {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtensionsLeft);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && moveScore <= KILLER_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesPlayed];
                lmr -= pvNode;
                double moveHistory = moveScore;

                if (moveScore == KILLER_SCORE || moveScore == COUNTERMOVE_SCORE)
                {
                    lmr--;
                    moveHistory = historyEntry->quietHistory(board);
                }
                else if (!isQuiet)
                    moveHistory = historyEntry->noisyHistory;

                lmr -= round(moveHistory / (double)lmrHistoryDivisor.value);
                lmr = std::clamp(lmr, 0, depth - 2);
            }

            score = -search(depth - 1 - lmr + extension, ply + 1, -alpha-1, -alpha, doubleExtensionsLeft);

            if (score > alpha && (score < beta || lmr > 0))
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtensionsLeft); 

            moveSearched:
            board.undoMove();
            if (isHardTimeUp()) return 0;

            if (ply == 0) movesNodes[move.getMoveEncoded()] += nodes - nodesBefore;

            if (score > bestScore) bestScore = score;

            if (score <= alpha) // Fail low
            {
                int index = isQuiet ? failLowQuiets++ : 256 - ++failLowNoisies;
                failLowsHistoryEntry[index] = historyEntry;
                continue;
            }

            alpha = score;
            bestMove = move;
            if (ply == 0) pvLines[0][0] = move;

            if (score < beta) continue;

            // Fail high / beta cutoff

            i32 historyBonus = min(historyMaxBonus.value, 
                                   historyBonusMultiplier.value * (depth-1));

            if (isQuiet)
            {
                killerMoves[ply] = move;

                Move lastMove = MOVE_NONE;
                if ((lastMove = board.lastMove()) != MOVE_NONE)
                    countermoves[(i8)stm][(u8)lastMove.pieceType()][lastMove.to()] = move;

                // Increase history of this fail high quiet move
                historyEntry->updateQuietHistory(board, historyBonus);

                // History malus: if this fail high is a quiet, decrease history of fail low quiets
                for  (int i = 0; i < failLowQuiets; i++)
                    failLowsHistoryEntry[i]->updateQuietHistory(board, -historyBonus);
            }
            else
                // Increaes history of this fail high noisy move
                historyEntry->updateNoisyHistory(board, historyBonus);

            // History malus: always decrease history of fail low noisy moves
            for (int i = 255, j = 0; j < failLowNoisies; j++, i--)
                failLowsHistoryEntry[i]->updateNoisyHistory(board, -historyBonus);

            break; // Fail high / beta cutoff
        }

        if (legalMovesPlayed == 0) return board.inCheck() ? -INF + ply : 0;

        if (!singular)
            tt.store(ttEntry, board.zobristHash(), depth, ply, bestScore, bestMove, originalAlpha, beta);    

        return bestScore;
    }

    inline i32 qSearch(u8 ply, i32 alpha, i32 beta)
    {
        // Quiescence search: search noisy moves until a 'quiet' position is reached

        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (board.isDraw()) return 0;

        if (ply >= maxDepth) return board.inCheck() ? 0 : board.evaluate();

        i32 eval = -INF; // eval is -INF in check
        if (!board.inCheck())
        {
            eval = board.evaluate();
            if (eval >= beta) return eval;
            if (eval > alpha) alpha = eval;
        }

        // if in check, generate all moves, else only noisy moves
        // never generate underpromotions
        MovesList moves = MovesList();
        board.pseudolegalMoves(moves, !board.inCheck(), false); 
        std::array<i32, 256> movesScores;
        scoreMoves(moves, movesScores, MOVE_NONE, killerMoves[ply]);
        
        u8 legalMovesPlayed = 0;
        i32 bestScore = eval;
        Move bestMove = MOVE_NONE;

        for (int i = 0; i < moves.size(); i++)
        {
            auto [move, moveScore] = incrementalSort(moves, movesScores, i);

            // SEE pruning (skip bad captures)
            if (!board.inCheck() && moveScore < 0) 
                continue;

            // skip illegal moves
            if (!board.makeMove(move)) continue; 

            legalMovesPlayed++;
            nodes++;

            i32 score = -qSearch(ply + 1, -beta, -alpha);
            board.undoMove();
            if (isHardTimeUp()) return 0;

            if (score <= bestScore) continue;

            bestScore = score;
            bestMove = move;

            if (bestScore >= beta) break;
            if (bestScore > alpha) alpha = bestScore;
        }

        if (legalMovesPlayed == 0 && board.inCheck()) 
            // checkmate
            return -INF + ply; 

        return bestScore;
    }

    const i32 GOOD_QUEEN_PROMO_SCORE = 1'600'000'000,
              GOOD_NOISY_SCORE       = 1'500'000'000,
              KILLER_SCORE           = 1'000'000'000,
              COUNTERMOVE_SCORE      = 500'000'000;

    // Most valuable victim     P    N    B    R    Q    K  NONE
    const i32 MVV_VALUES[7] = { 100, 300, 320, 500, 900, 0, 0 };

    inline void scoreMoves(MovesList &moves, std::array<i32, 256> &scores, Move ttMove, Move killer)
    {
        i8 stm = (i8)board.sideToMove();
        Move lastMove = MOVE_NONE;
        Move countermove = (lastMove = board.lastMove()) == MOVE_NONE
                           ? MOVE_NONE
                           : countermoves[stm][(u8)lastMove.pieceType()][lastMove.to()];

        for (int i = 0; i < moves.size(); i++)
        {
            Move move = moves[i];
            if (move == ttMove)
            {
                scores[i] = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();
            HistoryEntry *historyEntry = &historyTable[stm][(u8)move.pieceType()][move.to()];

            // Starzix doesn't generate underpromotions in search

            if (captured != PieceType::NONE || promotion == PieceType::QUEEN)
            {
                scores[i] = SEE(board, move) ? (promotion == PieceType::QUEEN
                                                ? GOOD_QUEEN_PROMO_SCORE : GOOD_NOISY_SCORE)
                                             : -GOOD_NOISY_SCORE;

                scores[i] += MVV_VALUES[(u8)captured];
                scores[i] += historyEntry->noisyHistory;
            }
            else if (move == killer)
                scores[i] = KILLER_SCORE;
            else if (move == countermove)
                scores[i] = COUNTERMOVE_SCORE;
            else
                scores[i] = historyEntry->quietHistory(board);
            
        }
    }

};