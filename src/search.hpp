#include "search_params.hpp"
#include "tt.hpp"
#include "see.hpp"
#include "history_entry.hpp"

std::array<std::array<u8, 256>, 256> LMR_TABLE; // [depth][moveIndex]

constexpr void initLmrTable()
{
    memset(LMR_TABLE.data(), 0, sizeof(LMR_TABLE));
    for (int depth = 1; depth < LMR_TABLE.size(); depth++)
        for (int move = 1; move < LMR_TABLE.size(); move++)
            LMR_TABLE[depth][move] = round(lmrBase.value + ln(depth) * ln(move) * lmrMultiplier.value);
}

struct PlyData {
    public:
    std::array<Move, 256> pvLine = { };
    u8 pvLength = 0;
    Move killer = MOVE_NONE;
    i32 eval = 0;
    MovesList moves = MovesList();
    std::array<i32, 256> movesScores = { };
};

class Searcher {
    public:

    Board board;

    private:

    u8 maxDepth;
    u64 nodes;
    u8 maxPlyReached;
    TT tt;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    u64 softMilliseconds, hardMilliseconds;
    u64 softNodes, hardNodes;
    bool hardTimeUp;

    std::array<PlyData, 256> pliesData;     // [ply]
    std::array<u64, 1ULL << 16> movesNodes; // [moveEncoded]

    // [color][pieceType][targetSquare]
    std::array<std::array<std::array<Move, 64>, 6>, 2> countermoves;
    std::array<std::array<std::array<HistoryEntry, 64>, 6>, 2> historyTable;

    const i64 OVERHEAD_MILLISECONDS = 10;

    public:

    inline Searcher() {  
        ucinewgame();
        nodes = 0;      
        resizeTT();
    }

    inline void ucinewgame() {
        startTime = std::chrono::steady_clock::now();
        board = START_BOARD;
        tt.resize();
        memset(pliesData.data(), 0, sizeof(pliesData));
        memset(movesNodes.data(), 0, sizeof(movesNodes));
        memset(countermoves.data(), 0, sizeof(countermoves));
        memset(historyTable.data(), 0, sizeof(historyTable));
    }
    
    inline u64 getNodes() { return nodes; }

    inline Move bestMoveRoot() {
        return pliesData[0].pvLine[0];
    }

    inline void resizeTT() { 
        tt.resize(); 
        std::cout << "TT size: " << ttSizeMB << " MB (" << tt.entries.size() << " entries)" << std::endl;
    }

    inline u64 millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - startTime) / std::chrono::milliseconds(1);
    }

    private:

    inline bool isHardTimeUp() {
        if (bestMoveRoot() == MOVE_NONE) return false;

        if (hardTimeUp || nodes >= hardNodes) return true;

        // Check time every 1024 nodes
        if ((nodes % 1024) != 0) return false;

        return hardTimeUp = (millisecondsElapsed() >= hardMilliseconds);
    }

    inline double bestMoveNodesFraction() {
        assert(bestMoveRoot() != MOVE_NONE);
        return (double)movesNodes[bestMoveRoot().encoded()] / (double)nodes;
    }

    public:

    inline std::pair<Move, i32> search(u8 maxDepth, i64 milliseconds, u64 incrementMs, u64 movesToGo, 
                                       bool isMoveTime, u64 softNodes, u64 hardNodes, bool printInfo)
    {
        // init and reset stuff
        startTime = std::chrono::steady_clock::now();
        this->maxDepth = maxDepth;
        nodes = maxPlyReached = 0;
        this->softNodes = softNodes;
        this->hardNodes = hardNodes;
        hardTimeUp = false;
        memset(movesNodes.data(), 0, sizeof(movesNodes));

        // clear best root move and pv lines
        pliesData[0].pvLine[0] = MOVE_NONE;
        for (int i = 0; i < pliesData.size(); i++) 
            pliesData[i].pvLength = 0;

        // Set time limits

        assert(movesToGo > 0);
        milliseconds = max((i64)0, milliseconds);
        u64 maxHardMilliseconds = max((i64)0, milliseconds - OVERHEAD_MILLISECONDS);

        if (isMoveTime || maxHardMilliseconds <= 0) {
            hardMilliseconds = maxHardMilliseconds;
            softMilliseconds = U64_MAX;
        }
        else {
            hardMilliseconds = maxHardMilliseconds * hardTimePercentage.value;
            softMilliseconds = (maxHardMilliseconds / movesToGo + incrementMs * 0.6666) * softTimePercentage.value;
            softMilliseconds = min(softMilliseconds, hardMilliseconds);
        }

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
            u64 msElapsed = millisecondsElapsed();

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

                std::cout << " pv " << bestMoveRoot().toUci();
                for (int i = 1; i < pliesData[0].pvLength; i++)
                    std::cout << " " << pliesData[0].pvLine[i].toUci();

                std::cout << std::endl;
            }

            // Check soft limits
            if (nodes >= softNodes
            || msElapsed >= (iterationDepth >= nodesTmMinDepth.value
                             ? softMilliseconds * nodesTmMultiplier.value
                               * (nodesTmBase.value - bestMoveNodesFraction()) 
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
        i32 bestScore = score;

        while (true)
        {
            score = search(depth, 0, alpha, beta, doubleExtensionsMax.value);

            if (isHardTimeUp()) return 0;

            if (score > bestScore) bestScore = score;

            if (score >= beta)
            {
                beta = min(beta + delta, INF);
                depth--;
            }
            else if (score <= alpha)
            {
                beta = (alpha + beta + bestScore) / 3;
                alpha = max(alpha - delta, -INF);
                depth = iterationDepth;
            }
            else
                break;

            delta *= aspDeltaMultiplier.value;
        }

        if (tt.age < 63) tt.age++;

        return score;
    }

    inline i32 search(i32 depth, u8 ply, i32 alpha, i32 beta, 
                      u8 doubleExtsLeft, bool singular = false)
    { 
        PlyData &plyData = pliesData[ply];
        plyData.pvLength = 0; // Ensure fresh PV

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (ply > 0 && !singular && board.isDraw()) return 0;

        if (depth > maxDepth) depth = maxDepth;

        // Probe TT
        TTEntry *ttEntry = tt.probe(board.zobristHash());
        bool ttHit = board.zobristHash() == ttEntry->zobristHash;

        // TT cutoff
        if (ttHit && ply > 0 && !singular
        && ttEntry->depth >= depth 
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        if (!singular) {
            plyData.eval = board.inCheck() ? 0 : board.evaluate();

            // If possible, use tt score as static eval
            /*
            if (ttHit
            && !board.inCheck()
            && (plyData.eval <= ttEntry->score || ttEntry->getBound() != Bound::LOWER)
            && (plyData.eval >= ttEntry->score || ttEntry->getBound() != Bound::UPPER))
                plyData.eval = ttEntry->score;
            */
        }

        if (ply >= maxDepth) return plyData.eval;

        Color stm = board.sideToMove();
        bool pvNode = beta > alpha + 1;

        if (!pvNode && !singular && !board.inCheck())
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth.value && plyData.eval >= beta + depth * rfpDepthMultiplier.value)
                return plyData.eval;

            // Razoring
            if (depth <= razoringMaxDepth.value 
            && plyData.eval + depth * razoringDepthMultiplier.value < alpha)
            {
                i32 score = qSearch(ply, alpha, beta);
                if (score <= alpha) return score;
            }

            // NMP (Null move pruning)
            if (depth >= nmpMinDepth.value 
            && board.lastMove() != MOVE_NONE 
            && plyData.eval >= beta
            && !(ttHit && ttEntry->getBound() == Bound::UPPER && ttEntry->score < beta)
            && board.hasNonPawnMaterial(stm))
            {
                board.makeMove(MOVE_NONE, &tt);

                i32 nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value
                               - min((plyData.eval-beta) / nmpEvalBetaDivisor.value, nmpEvalBetaMax.value);

                i32 score = -search(nmpDepth, ply + 1, -beta, -alpha, doubleExtsLeft);

                board.undoMove();

                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }
        }

        Move ttMove = ttHit ? ttEntry->bestMove : MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth.value && ttMove == MOVE_NONE)
            depth--;

        // genenerate and score all moves except underpromotions
        if (!singular) {
            board.pseudolegalMoves(plyData.moves, false, false);
            scoreMoves(plyData, ttMove);
        }

        bool improving = ply > 1 && plyData.eval > pliesData[ply-2].eval 
                         && !board.inCheck() && !board.inCheck2PliesAgo();

        u8 legalMovesPlayed = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Fail low quiets at beginning of array, fail low noisy moves at the end
        std::array<HistoryEntry*, 256> failLowsHistoryEntry;
        u8 failLowQuiets = 0, failLowNoisies = 0;

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = incrementalSort(plyData.moves, plyData.movesScores, i);

            // Don't search TT move in singular search
            if (move == ttMove && singular) continue;

            bool isQuiet = !board.isCapture(move) && move.promotion() == PieceType::NONE;

            if (bestScore > -MIN_MATE_SCORE && !pvNode && !board.inCheck() && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesPlayed >= lmpMinMoves.value + depth * depth * lmpDepthMultiplier.value)
                    break;

                i32 lmrDepth = max(0, depth - (i32)LMR_TABLE[depth][legalMovesPlayed+1]);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth.value && alpha < MIN_MATE_SCORE
                && alpha > plyData.eval + fpBase.value + lmrDepth * fpMultiplier.value)
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth.value 
                && !SEE(board, move, 
                        depth * (isQuiet ? seeQuietThreshold.value : depth * seeNoisyThreshold.value)))
                    continue;
            }

            // skip illegal moves
            if (!board.makeMove(move, &tt)) continue;

            i32 extension = 0;
            if (ply == 0) goto skipExtensions;

            // SE (Singular extensions)
            if (move == ttMove
            && depth >= singularMinDepth.value
            && abs(ttEntry->score) < MIN_MATE_SCORE
            && (i32)ttEntry->depth >= depth - singularDepthMargin.value
            && ttEntry->getBound() != Bound::UPPER)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                // Undo TT move we just made
                BoardState boardState = board.getState();
                board.undoMove();

                i32 singularBeta = max(-INF, ttEntry->score - depth * singularBetaMultiplier.value);

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           doubleExtsLeft, true);

                board.pushState(boardState, &tt); // Make the TT move again

                // Double extension
                if (singularScore < singularBeta - doubleExtensionMargin.value 
                && !pvNode && doubleExtsLeft > 0)
                {
                    // singularScore is way lower than TT score
                    // TT move is probably MUCH better than all others, so extend its search by 2 plies
                    extension = 2;
                    doubleExtsLeft--;
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
            u8 pt = (u8)move.pieceType();
            HistoryEntry *historyEntry = &historyTable[(int)stm][pt][move.to()];

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;
            if (legalMovesPlayed == 1)
            {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && moveScore < COUNTERMOVE_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesPlayed];
                lmr -= pvNode;    // reduce pv nodes less
                lmr -= improving; // reduce less if were improving
                
                // reduce moves with good history less and vice versa
                lmr -= round(isQuiet ? (float)moveScore / (float)lmrQuietHistoryDiv.value
                                     : (float)historyEntry->noisyHistory / (float)lmrNoisyHistoryDiv.value);

                lmr = std::clamp(lmr, 0, depth - 2); // dont extend or reduce into qsearch
            }

            score = -search(depth - 1 - lmr + extension, ply + 1, -alpha-1, -alpha, doubleExtsLeft);

            if (score > alpha && (pvNode || lmr > 0))
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft); 

            moveSearched:
            board.undoMove();
            if (isHardTimeUp()) return 0;

            if (ply == 0) movesNodes[move.encoded()] += nodes - nodesBefore;

            if (score > bestScore) bestScore = score;

            if (score <= alpha) // Fail low
            {
                int index = isQuiet ? failLowQuiets++ : 256 - ++failLowNoisies;
                failLowsHistoryEntry[index] = historyEntry;
                continue;
            }

            alpha = score;
            bestMove = move;
            bound = Bound::EXACT;
            
            // Update pv line
            if (pvNode) {
                plyData.pvLength = 1 + pliesData[ply+1].pvLength;
                plyData.pvLine[0] = move;

                memcpy(&(plyData.pvLine[1]),                      // dst
                       pliesData[ply+1].pvLine.data(),            // src
                       pliesData[ply+1].pvLength * sizeof(Move)); // size
            }

            if (score < beta) continue;

            // Fail high / beta cutoff

            bound = Bound::LOWER;

            i32 historyBonus = min(historyMaxBonus.value, 
                                   historyBonusMultiplier.value * (depth-1));

            if (isQuiet) {
                // This fail high quiet is now a killer move
                plyData.killer = move; 

                // This fail high quiet is now a countermove
                Move lastMove;
                if ((lastMove = board.lastMove()) != MOVE_NONE)
                    countermoves[(int)stm][(int)lastMove.pieceType()][lastMove.to()] = move;

                // Increase history of this fail high quiet move
                historyEntry->updateQuietHistory(board, historyBonus);

                // History malus: this fail high is a quiet, so decrease history of fail low quiets
                for (int i = 0; i < failLowQuiets; i++)
                    failLowsHistoryEntry[i]->updateQuietHistory(board, -historyBonus);
            }
            else
                // Increaes history of this fail high noisy move
                historyEntry->updateNoisyHistory(historyBonus);

            // History malus: always decrease history of fail low noisy moves
            for (int i = 255, j = 0; j < failLowNoisies; j++, i--)
                failLowsHistoryEntry[i]->updateNoisyHistory(-historyBonus);

            break; // Fail high / beta cutoff
        }

        if (legalMovesPlayed == 0) 
            return board.inCheck() ? -INF + ply : 0;

        if (!singular)
            tt.store(ttEntry, board.zobristHash(), depth, ply, bestScore, bestMove, bound);    

        return bestScore;
    }

    // Quiescence search
    inline i32 qSearch(u8 ply, i32 alpha, i32 beta)
    {
        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (ply > 0 && board.isDraw()) return 0;

        // Probe TT
        TTEntry *ttEntry = tt.probe(board.zobristHash());
        bool ttHit = ttEntry->zobristHash == board.zobristHash();

        // TT cutoff
        if (ttHit && ply > 0
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        PlyData &plyData = pliesData[ply];

        if (board.inCheck())
            plyData.eval = 0;
        else {
            plyData.eval = board.evaluate();

            // If possible, use tt score as static eval
            /*
            if (ttHit
            && (plyData.eval <= ttEntry->score || ttEntry->getBound() != Bound::LOWER)
            && (plyData.eval >= ttEntry->score || ttEntry->getBound() != Bound::UPPER))
                plyData.eval = ttEntry->score;
            */

            if (plyData.eval >= beta) return plyData.eval; 
            if (plyData.eval > alpha) alpha = plyData.eval;
        }

        if (ply >= maxDepth) return plyData.eval;

        // if in check, generate all moves, else only noisy moves
        // never generate underpromotions
        board.pseudolegalMoves(plyData.moves, !board.inCheck(), false);
        scoreMoves(plyData, ttHit ? ttEntry->bestMove : MOVE_NONE);
        
        u8 legalMovesPlayed = 0;
        i32 bestScore = board.inCheck() ? -INF : plyData.eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = incrementalSort(plyData.moves, plyData.movesScores, i);

            // SEE pruning (skip bad noisy moves)
            if (!board.inCheck() && moveScore < 0) break;

            // skip illegal moves
            if (!board.makeMove(move, &tt)) continue; 

            legalMovesPlayed++;
            nodes++;

            i32 score = -qSearch(ply + 1, -beta, -alpha);
            board.undoMove();
            if (isHardTimeUp()) return 0;

            if (score <= bestScore) continue;

            bestScore = score;
            bestMove = move;

            if (bestScore >= beta) {
                bound = Bound::LOWER;
                break;
            }
            
            if (bestScore > alpha) alpha = bestScore;
        }

        if (legalMovesPlayed == 0 && board.inCheck()) 
            // checkmate
            return -INF + ply; 

        tt.store(ttEntry, board.zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    const i32 GOOD_QUEEN_PROMO_SCORE = 1'600'000'000,
              GOOD_NOISY_SCORE       = 1'500'000'000,
              KILLER_SCORE           = 1'000'000'000,
              COUNTERMOVE_SCORE      = 500'000'000;

    inline void scoreMoves(PlyData &plyData, Move ttMove)
    {
        int stm = (int)board.sideToMove();
        Move lastMove;
        Move countermove = (lastMove = board.lastMove()) == MOVE_NONE
                           ? MOVE_NONE
                           : countermoves[stm][(int)lastMove.pieceType()][lastMove.to()];

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            Move move = plyData.moves[i];

            if (move == ttMove) {
                plyData.movesScores[i] = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();
            u8 pt = (u8)move.pieceType();
            HistoryEntry *historyEntry = &historyTable[stm][pt][move.to()];

            // Starzix doesn't generate underpromotions in search

            if (captured != PieceType::NONE)
            {
                plyData.movesScores[i] = SEE(board, move) 
                                         ? (promotion == PieceType::QUEEN
                                            ? GOOD_QUEEN_PROMO_SCORE 
                                            : GOOD_NOISY_SCORE)
                                         : -GOOD_NOISY_SCORE;

                plyData.movesScores[i] += ((i32)captured + 1) * 1'000'000; // MVV (most valuable victim)
                plyData.movesScores[i] += historyEntry->noisyHistory;
            }
            else if (promotion == PieceType::QUEEN)
            {
                plyData.movesScores[i] = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE : -GOOD_NOISY_SCORE;
                plyData.movesScores[i] += historyEntry->noisyHistory;
            }
            else if (move == plyData.killer)
                plyData.movesScores[i] = KILLER_SCORE;
            else if (move == countermove)
                plyData.movesScores[i] = COUNTERMOVE_SCORE;
            else
                plyData.movesScores[i] = historyEntry->quietHistory(board);
            
        }
    }

}; // class Searcher