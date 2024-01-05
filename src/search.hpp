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
                                 : search(iterationDepth, 0, -INF, INF);

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
            score = search(depth, 0, alpha, beta);

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

    inline i32 search(i32 depth, u8 ply, i32 alpha, i32 beta)
    { 
        // Check extension
        if (board.inCheck()) depth = depth < 0 ? 1 : depth + 1;

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (ply > 0 && board.isDraw()) return 0;

        if (ply >= maxDepth) return board.inCheck() ? 0 : board.evaluate();

        if (depth > maxDepth) depth = maxDepth;

        auto [ttEntry, cutoff] = tt.probe(board.zobristHash(), depth, ply, alpha, beta);
        if (cutoff) return ttEntry->adjustedScore(ply);

        Color stm = board.sideToMove();
        bool pvNode = beta > alpha + 1;
        i32 eval = board.inCheck() ? 0 : board.evaluate();

        if (!pvNode && !board.inCheck())
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth.value && eval >= beta + depth * rfpDepthMultiplier.value)
                return eval;

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth.value && eval >= beta
            && board.lastMove() != MOVE_NONE && board.hasNonPawnMaterial(stm))
            {
                board.makeMove(MOVE_NONE);
                i32 nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value;
                i32 score = -search(nmpDepth, ply + 1, -beta, -alpha);
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

        MovesList moves = MovesList();
        board.pseudolegalMoves(moves, false, false);
        std::array<i32, 256> movesScores;
        scoreMoves(moves, movesScores, ttMove, killerMoves[ply]);

        u8 legalMovesPlayed = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        i32 originalAlpha = alpha;
        std::array<HistoryEntry*, 256> failLowQuietsHistoryEntry;
        u8 failLowQuiets = 0;

        for (int i = 0; i < moves.size(); i++)
        {
            auto [move, moveScore] = incrementalSort(moves, movesScores, i);
            bool isQuiet = !board.isCapture(move) && move.promotion() == PieceType::NONE;

            if (bestScore > -MIN_MATE_SCORE && !pvNode && !board.inCheck() && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (depth <= lmpMaxDepth.value
                && legalMovesPlayed >= lmpMinMoves.value + depth * depth * lmpDepthMultiplier.value)
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth.value 
                && !SEE(board, move, 
                        depth * (isQuiet ? seeQuietThreshold.value : depth * seeNoisyThreshold.value)))
                    continue;
            }

            // skip illegal moves
            if (!board.makeMove(move)) continue;

            legalMovesPlayed++;
            u64 nodesBefore = nodes;
            nodes++;
            u8 pt = (u8)move.pieceType();
            Square targetSq = move.to();

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;
            if (legalMovesPlayed == 1)
            {
                score = -search(depth - 1, ply + 1, -beta, -alpha);
                goto moveSearched;
            }

            if (depth >= 3 && moveScore <= KILLER_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesPlayed];
                lmr -= pvNode;

                double quietHist = 0;
                if (!isQuiet) goto lmrAdjusted;

                if (moveScore == KILLER_SCORE || moveScore == COUNTERMOVE_SCORE)
                {
                    lmr--;
                    quietHist = historyTable[(i8)stm][pt][targetSq].quietHistory(board);
                }
                else
                    quietHist = moveScore;
                lmr -= round(quietHist / (double)lmrHistoryDivisor.value);

                lmrAdjusted:
                lmr = std::clamp(lmr, 0, depth - 2);
            }

            
            score = -search(depth - 1 - lmr, ply + 1, -alpha-1, -alpha);
            if (score > alpha && (score < beta || lmr > 0))
                score = -search(depth - 1, ply + 1, -beta, -alpha); 

            moveSearched:
            board.undoMove();
            if (isHardTimeUp()) return 0;

            if (ply == 0) movesNodes[move.getMoveEncoded()] += nodes - nodesBefore;

            if (score > bestScore) bestScore = score;

            if (score <= alpha) // Fail low
            {
                failLowQuietsHistoryEntry[failLowQuiets++] 
                    = &historyTable[(i8)stm][pt][targetSq];
                continue;
            }

            alpha = score;
            bestMove = move;
            if (ply == 0) pvLines[0][0] = move;

            if (score < beta) continue;

            // Fail high / beta cutoff

            if (isQuiet)
            {
                killerMoves[ply] = move;

                Move lastMove = MOVE_NONE;
                if ((lastMove = board.lastMove()) != MOVE_NONE)
                    countermoves[(i8)stm][(u8)lastMove.pieceType()][lastMove.to()] = move;

                i32 historyBonus = min(historyMaxBonus.value, 
                                       historyBonusMultiplier.value * (depth-1));

                // Increase history of this fail high quiet move
                historyTable[(i8)stm][pt][targetSq].updateQuietHistory(board, historyBonus);

                // History malus: if this fail high is a quiet, decrease history of fail low quiets
                for  (int i = 0; i < failLowQuiets; i++)
                    failLowQuietsHistoryEntry[i]->updateQuietHistory(board, -historyBonus);
            }

            break; // Fail high / beta cutoff
        }

        if (legalMovesPlayed == 0) return board.inCheck() ? -INF + ply : 0;

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
            if (!board.inCheck() && moveScore <= -GOOD_NOISY_SCORE + 1000) 
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

    const i32 GOOD_NOISY_SCORE  = 1'500'000'000,
              KILLER_SCORE      = 1'000'000'000,
              COUNTERMOVE_SCORE = 500'000'000;

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
                scores[i] = 2'000'000'000;
                continue;
            }

            scores[i] = 0;
            PieceType captured = board.captured(move);
            u8 pt = (u8)move.pieceType();

            if (captured != PieceType::NONE)
                scores[i] += 100 * (i32)captured - (i32)move.pieceType();
            if (move.promotion() != PieceType::NONE)
                scores[i] += SEE(board, move) ? GOOD_NOISY_SCORE + 100'000'000
                                              : -GOOD_NOISY_SCORE;
            else if (captured != PieceType::NONE)
                scores[i] += SEE(board, move) ? GOOD_NOISY_SCORE : -GOOD_NOISY_SCORE;
            else if (move == killer)
                scores[i] = KILLER_SCORE;
            else if (move == countermove)
                scores[i] = COUNTERMOVE_SCORE;
            else
                scores[i] = historyTable[stm][pt][move.to()].quietHistory(board);
            
        }
    }

};