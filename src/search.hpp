// clang-format off

#pragma once

#include "search_params.hpp"
#include "tt_entry.hpp"
#include "nnue.hpp"
#include "see.hpp"
#include "history_entry.hpp"

constexpr u8 MAX_DEPTH = 100;
constexpr i64 OVERHEAD_MILLISECONDS = 10;

std::array<std::array<u8, 256>, MAX_DEPTH> LMR_TABLE; // [depth][moveIndex]

constexpr void initLmrTable()
{
    memset(LMR_TABLE.data(), 0, sizeof(LMR_TABLE));
    for (u64 depth = 1; depth < LMR_TABLE.size(); depth++)
        for (u64 move = 1; move < LMR_TABLE[0].size(); move++)
            LMR_TABLE[depth][move] = round(lmrBase.value + ln(depth) * ln(move) * lmrMultiplier.value);
}

struct PlyData {
    public:
    std::array<Move, MAX_DEPTH+1> pvLine = { };
    u8 pvLength = 0;
    Move killer = MOVE_NONE;
    i32 eval = INF;
    MovesList moves = MovesList();
    std::array<i32, 256> movesScores = { };
};

class Searcher {
    public:

    Board board = START_BOARD;

    private:

    u8 maxDepth = MAX_DEPTH;
    u64 nodes = 0;
    u8 maxPlyReached = 0;

    std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();

    u64 softMilliseconds = U64_MAX, 
        hardMilliseconds = U64_MAX,
        softNodes = U64_MAX,
        hardNodes = U64_MAX;

    bool hardTimeUp = false;

    std::array<PlyData, MAX_DEPTH+1> pliesData = {}; // [ply]

    std::array<Accumulator, MAX_DEPTH+1> accumulators = { START_POS_ACCUMULATOR };
    u8 accumulatorIdx = 0;

    std::vector<TTEntry> tt = std::vector<TTEntry>(32 * 1024 * 1024 / sizeof(TTEntry));

    std::array<u64, 1ULL << 17> movesNodes = {}; // [move]
    std::array<std::array<Move, 1ULL << 17>, 2> countermoves = {}; // [nstm][lastMove]

    // [color][pieceType][targetSquare]
    std::array<std::array<std::array<HistoryEntry, 64>, 6>, 2> historyTable = {};

    public:

    inline void ucinewgame() {
        startTime = std::chrono::steady_clock::now();

        board = START_BOARD;
        accumulators = { START_POS_ACCUMULATOR };
        accumulatorIdx = 0;

        nodes = maxPlyReached = 0;
        hardTimeUp = false;
        pliesData = {};
        memset(tt.data(), 0, tt.size() * sizeof(TTEntry));
        movesNodes = {};
        countermoves = {};
        historyTable = {};
    }
    
    inline u64 getNodes() { return nodes; }

    inline Move bestMoveRoot() {
        return pliesData[0].pvLine[0];
    }

    inline void resizeTT(u64 newSizeMB) { 
        tt.clear();
        u64 numEntries = newSizeMB * (u64)1024 * (u64)1024 / (u64)sizeof(TTEntry); 
        tt.resize(numEntries);
    }

    inline void printTTSize() {
        double bytes = (u64)tt.size() * (u64)sizeof(TTEntry);
        double megabytes = bytes / (1024.0 * 1024.0);

        std::cout << "TT size: " << round(megabytes) << " MB"
                  << " (" << tt.size() << " entries)" 
                  << std::endl;
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

    inline void makeMove(Move move, u8 newPly)
    {
        // If not a special move, we can probably correctly predict the zobrist hash after the move
        // and prefetch the TT entry
        if (move.flag() <= Move::KING_FLAG) 
        {
            auto ttEntryIdx = TTEntryIndex(board.roughHashAfter(move), tt.size());
            __builtin_prefetch(&tt[ttEntryIdx]);
        }

        board.makeMove(move);
        nodes++;

        if (move != MOVE_NONE)
            accumulators[++accumulatorIdx].updated = false;
        
        pliesData[newPly].pvLength = 0;
        pliesData[newPly].eval = INF;

        // Update seldepth
        if (newPly > maxPlyReached) maxPlyReached = newPly;
    }

    inline void setEval(PlyData &plyData) 
    {
        assert(accumulators[accumulatorIdx == 0 ? 0 : accumulatorIdx - 1].updated);

        if (!accumulators[accumulatorIdx].updated) 
            accumulators[accumulatorIdx].update(
                accumulators[accumulatorIdx-1], board.oppSide(), board.lastMove(), board.captured());

        if (plyData.eval == INF && !board.inCheck())
            plyData.eval = evaluate(accumulators[accumulatorIdx], board, true);
    }

    public:

    inline std::pair<Move, i32> search(u8 maxDepth, i64 milliseconds, u64 incrementMs, u64 movesToGo, 
                                       bool isMoveTime, u64 softNodes, u64 hardNodes, bool printInfo)
    {
        // init/reset stuff

        startTime = std::chrono::steady_clock::now();

        this->maxDepth = maxDepth = std::clamp(maxDepth, (u8)1, MAX_DEPTH);
        this->softNodes = softNodes;
        this->hardNodes = hardNodes;

        nodes = maxPlyReached = 0;
        hardTimeUp = false;

        pliesData[0] = PlyData();

        accumulatorIdx = 0;
        accumulators[0] = Accumulator(board);
        setEval(pliesData[0]);

        movesNodes = {};

        // Set time limits

        assert(movesToGo > 0);
        milliseconds = std::max((i64)0, milliseconds);
        u64 maxHardMilliseconds = std::max((i64)0, milliseconds - OVERHEAD_MILLISECONDS);

        if (isMoveTime || maxHardMilliseconds <= 0) {
            hardMilliseconds = maxHardMilliseconds;
            softMilliseconds = U64_MAX;
        }
        else {
            hardMilliseconds = maxHardMilliseconds * hardTimePercentage.value;
            softMilliseconds = (maxHardMilliseconds / movesToGo + incrementMs * 0.6666) * softTimePercentage.value;
            softMilliseconds = std::min(softMilliseconds, hardMilliseconds);
        }

        // ID (Iterative deepening)
        i32 score = 0;
        for (i32 iterationDepth = 1; iterationDepth <= maxDepth; iterationDepth++)
        {
            maxPlyReached = 0;

            i32 iterationScore = iterationDepth >= aspMinDepth.value 
                                 ? aspiration(iterationDepth, score)
                                 : search(iterationDepth, 0, -INF, INF, 
                                          doubleExtensionsMax.value, false);
                                 
            if (isHardTimeUp()) break;

            score = iterationScore;
            u64 msElapsed = millisecondsElapsed();

            if (printInfo) {
                std::cout << "info depth " << iterationDepth
                          << " seldepth " << (int)maxPlyReached
                          << " time " << msElapsed
                          << " nodes " << nodes
                          << " nps " << nodes * 1000 / std::max(msElapsed, (u64)1);

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
        i32 alpha = std::max(-INF, score - delta);
        i32 beta = std::min(INF, score + delta);
        i32 depth = iterationDepth;
        i32 bestScore = score;

        while (true) {
            score = search(depth, 0, alpha, beta, doubleExtensionsMax.value, false);

            if (isHardTimeUp()) return 0;

            if (score > bestScore) bestScore = score;

            if (score >= beta)
            {
                beta = std::min(beta + delta, INF);
                if (depth > 1) depth--;
            }
            else if (score <= alpha)
            {
                beta = (alpha + beta + bestScore) / 3;
                alpha = std::max(alpha - delta, -INF);
                depth = iterationDepth;
            }
            else
                break;

            delta *= aspDeltaMultiplier.value;
        }

        return score;
    }

    inline i32 search(i32 depth, u8 ply, i32 alpha, i32 beta, 
                      u8 doubleExtsLeft, bool cutNode, bool singular = false)
    { 
        assert(ply <= maxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta >= -INF && beta <= INF);
        assert(alpha < beta);

        if (depth <= 0) return qSearch(ply, alpha, beta);

        if (isHardTimeUp()) return 0;

        if (depth > maxDepth) depth = maxDepth;

        bool pvNode = beta > alpha + 1;
        if (pvNode) assert(!cutNode);

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(board.zobristHash(), tt.size());
        TTEntry ttEntry = tt[ttEntryIdx];
        bool ttHit = board.zobristHash() == ttEntry.zobristHash;

        // TT cutoff
        if (ttHit && !pvNode && !singular
        && ttEntry.depth >= depth 
        && (ttEntry.getBound() == Bound::EXACT
        || (ttEntry.getBound() == Bound::LOWER && ttEntry.score >= beta) 
        || (ttEntry.getBound() == Bound::UPPER && ttEntry.score <= alpha)))
            return ttEntry.adjustedScore(ply);

        if (ply >= maxDepth && board.inCheck()) return 0;

        PlyData &plyData = pliesData[ply];
        setEval(plyData);

        if (ply >= maxDepth) return plyData.eval;

        Color stm = board.sideToMove();

        // If in check 2 plies ago, then pliesData[ply-2].eval is INF, and improving is false
        bool improving = ply > 1 && !board.inCheck() && plyData.eval > pliesData[ply-2].eval;

        if (!pvNode && !singular && !board.inCheck())
        {
            // RFP (Reverse futility pruning) / Static NMP
            if (depth <= rfpMaxDepth.value 
            && plyData.eval >= beta + (depth - improving) * rfpDepthMultiplier.value)
                return (plyData.eval + beta) / 2;

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
            && !(ttHit && ttEntry.getBound() == Bound::UPPER && ttEntry.score < beta)
            && board.hasNonPawnMaterial(stm))
            {
                makeMove(MOVE_NONE, ply + 1);

                i32 score = 0;

                if (!board.isDraw()) {
                    i32 nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value
                                   - std::min((plyData.eval-beta) / nmpEvalBetaDivisor.value, nmpEvalBetaMax.value);

                    score = -search(nmpDepth, ply + 1, -beta, -alpha, doubleExtsLeft, !cutNode);
                }

                board.undoMove();

                if (score >= MIN_MATE_SCORE) return beta;
                if (score >= beta) return score;
            }

            if (isHardTimeUp()) return 0;
        }

        if (!ttHit) ttEntry.move = MOVE_NONE;

        // IIR (Internal iterative reduction)
        if (depth >= iirMinDepth.value && ttEntry.move == MOVE_NONE && (pvNode || cutNode))
            depth--;

        // genenerate and score all moves except underpromotions
        if (!singular) {
            board.pseudolegalMoves(plyData.moves, false, false);
            scoreMoves(plyData, ttEntry.move);
        }

        u64 pinned = board.pinned();
        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Fail low quiets at beginning of array, fail low noisy moves at the end
        std::array<HistoryEntry*, 256> failLowsHistoryEntry;
        int failLowQuiets = 0, failLowNoisies = 0;

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = plyData.moves.incrementalSort(plyData.movesScores, i);

            // Don't search TT move in singular search
            if (move == ttEntry.move && singular) continue;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            legalMovesSeen++;
            bool isQuiet = !board.isCapture(move) && move.promotion() == PieceType::NONE;

            int pt = (int)move.pieceType();
            HistoryEntry *historyEntry = &historyTable[(int)stm][pt][move.to()];

            if (bestScore > -MIN_MATE_SCORE && !pvNode && !board.inCheck() && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves.value + depth * depth * lmpDepthMultiplier.value / (improving ? 1 : 2))
                    break;

                i32 lmrDepth = std::max(0, depth - (i32)LMR_TABLE[depth][legalMovesSeen] - !improving);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth.value 
                && alpha < MIN_MATE_SCORE
                && alpha > plyData.eval + fpBase.value + lmrDepth * fpMultiplier.value)
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth.value) 
                {
                    i32 threshold = isQuiet ? depth * seeQuietThreshold.value 
                                              - moveScore / seePruningQuietHistoryDiv.value
                                            : depth * depth * seeNoisyThreshold.value 
                                              - historyEntry->noisyHistory / seePruningNoisyHistoryDiv.value;

                    if (!SEE(board, move, std::min(threshold, -1))) continue;
                }
            }

            i32 extension = 0;
            if (ply == 0) goto skipExtensions;

            // SE (Singular extensions)
            if (move == ttEntry.move
            && depth >= singularMinDepth.value
            && abs(ttEntry.score) < MIN_MATE_SCORE
            && (i32)ttEntry.depth >= depth - singularDepthMargin.value
            && ttEntry.getBound() != Bound::UPPER)
            {
                // Singular search: before searching any move, 
                // search this node at a shallower depth with TT move excluded

                i32 singularBeta = std::max(-INF, (i32)ttEntry.score - i32(depth * singularBetaMultiplier.value));

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           doubleExtsLeft, cutNode, true);

                // Double extension
                if (singularScore < singularBeta - doubleExtensionMargin.value 
                && !pvNode 
                && doubleExtsLeft > 0)
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
                // Negative extensions
                else {
                    // some other move is probably better than TT move, so reduce TT move search
                    extension -= ttEntry.score >= beta; 
                    // reduce TT move search more if we expect to fail high
                    extension -= cutNode; 
                }
            }
            // Check extension if no singular extensions
            else if (board.inCheck())
                extension = 1;

            skipExtensions:

            u64 nodesBefore = nodes;
            makeMove(move, ply + 1);

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;

            if (board.isDraw()) goto moveSearched;

            if (legalMovesSeen == 1) {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft, false);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && legalMovesSeen >= lmrMinMoves.value && moveScore < COUNTERMOVE_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesSeen];
                lmr -= pvNode;          // reduce pv nodes less
                lmr -= board.inCheck(); // reduce less if move gives check
                lmr -= improving;       // reduce less if were improving
                lmr += 2 * cutNode;     // reduce more if we expect to fail high
                
                // reduce moves with good history less and vice versa
                lmr -= round(isQuiet ? (float)moveScore / (float)lmrQuietHistoryDiv.value
                                     : (float)historyEntry->noisyHistory / (float)lmrNoisyHistoryDiv.value);

                lmr = std::clamp(lmr, 0, depth - 2); // dont extend or reduce into qsearch
            }

            score = -search(depth - 1 - lmr + extension, ply + 1, -alpha-1, -alpha, doubleExtsLeft, true);

            if (score > alpha && lmr > 0)
                score = -search(depth - 1 + extension, ply + 1, -alpha-1, -alpha, doubleExtsLeft, !cutNode); 

            if (score > alpha && pvNode)
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft, false);

            moveSearched:

            board.undoMove();
            accumulatorIdx--;

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

            i32 historyBonus = std::min(historyBonusMax.value, 
                                        depth * historyBonusMultiplier.value - historyBonusOffset.value);

            i32 historyMalus = -std::min(historyMalusMax.value,
                                         depth * historyMalusMultiplier.value - historyMalusOffset.value);

            if (isQuiet) {
                // This fail high quiet is now a killer move
                plyData.killer = move; 

                // This fail high quiet is now a countermove
                if (board.lastMove() != MOVE_NONE) {
                    int nstm = (int)board.oppSide();
                    countermoves[nstm][board.lastMove().encoded()] = move;
                }

                // Increase history of this fail high quiet move
                historyEntry->updateQuietHistory(board, historyBonus);

                // History malus: this fail high is a quiet, so decrease history of fail low quiets
                for (int i = 0; i < failLowQuiets; i++)
                    failLowsHistoryEntry[i]->updateQuietHistory(board, historyMalus);
            }
            else
                // Increaes history of this fail high noisy move
                historyEntry->updateNoisyHistory(historyBonus);

            // History malus: always decrease history of fail low noisy moves
            for (int i = 255, j = 0; j < failLowNoisies; j++, i--)
                failLowsHistoryEntry[i]->updateNoisyHistory(historyMalus);

            break; // Fail high / beta cutoff
        }

        if (legalMovesSeen == 0) 
            return board.inCheck() ? -INF + (i32)ply : 0;

        if (!singular)
            tt[ttEntryIdx].update(board.zobristHash(), depth, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    // Quiescence search
    inline i32 qSearch(u8 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0 && ply <= maxDepth);
        assert(alpha >= -INF && alpha <= INF);
        assert(beta >= -INF && beta <= INF);
        assert(alpha < beta);

        if (isHardTimeUp()) return 0;

        // Probe TT
        auto ttEntryIdx = TTEntryIndex(board.zobristHash(), tt.size());
        TTEntry *ttEntry = &tt[ttEntryIdx];
        bool ttHit = board.zobristHash() == ttEntry->zobristHash;

        // TT cutoff
        if (ttHit
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        if (ply >= maxDepth && board.inCheck()) return 0;

        PlyData &plyData = pliesData[ply];
        setEval(plyData);

        if (!board.inCheck()) 
        {
            if (ply >= maxDepth || plyData.eval >= beta) 
                return plyData.eval; 

            if (plyData.eval > alpha) alpha = plyData.eval;
        }

        // if in check, generate all moves, else only noisy moves
        // never generate underpromotions
        board.pseudolegalMoves(plyData.moves, !board.inCheck(), false);
        scoreMoves(plyData, ttHit ? ttEntry->move : MOVE_NONE);
        
        u64 pinned = board.pinned();
        int legalMovesPlayed = 0;
        i32 bestScore = board.inCheck() ? -INF : plyData.eval;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = plyData.moves.incrementalSort(plyData.movesScores, i);

            // SEE pruning (skip bad noisy moves)
            if (!board.inCheck() && moveScore < 0) break;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            makeMove(move, ply + 1);
            legalMovesPlayed++;

            i32 score = board.isDraw() ? 0 : -qSearch(ply + 1, -beta, -alpha);

            board.undoMove();
            accumulatorIdx--;

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
            return -INF + (i32)ply; 

        ttEntry->update(board.zobristHash(), 0, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    const i32 GOOD_QUEEN_PROMO_SCORE = 1'600'000'000,
              GOOD_NOISY_SCORE       = 1'500'000'000,
              KILLER_SCORE           = 1'000'000'000,
              COUNTERMOVE_SCORE      = 500'000'000;

    inline void scoreMoves(PlyData &plyData, Move ttMove)
    {
        int stm = (int)board.sideToMove();
        int nstm = (int)board.oppSide();

        Move countermove = board.lastMove() == MOVE_NONE
                           ? MOVE_NONE : countermoves[nstm][board.lastMove().encoded()];

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            Move move = plyData.moves[i];

            if (move == ttMove) {
                plyData.movesScores[i] = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();
            int pt = (int)move.pieceType();
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