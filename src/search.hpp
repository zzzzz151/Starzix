// clang-format off

#pragma once

#include "search_params.hpp"
#include "tt.hpp"
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
    i32 eval = 0;
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

    std::array<u64, 1ULL << 17> movesNodes = {}; // [moveEncoded]

    // [color][pieceType][targetSquare]
    std::array<std::array<std::array<Move, 64>, 6>, 2> countermoves = {};
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

        pliesData[0].pvLine[0] = MOVE_NONE;
        for (u64 i = 0; i < pliesData.size(); i++) 
            pliesData[i].pvLength = 0;

        accumulators[0] = Accumulator(board);
        accumulatorIdx = 0;

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
                                 : search(iterationDepth, 0, -INF, INF, doubleExtensionsMax.value);
                                 
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
            score = search(depth, 0, alpha, beta, doubleExtensionsMax.value);

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
        TTEntry *ttEntry = probeTT(tt, board.zobristHash());
        bool ttHit = board.zobristHash() == ttEntry->zobristHash;

        // TT cutoff
        if (ttHit && ply > 0 && !singular
        && ttEntry->depth >= depth 
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        Color stm = board.sideToMove();

        if (!singular) {
            if (!accumulators[accumulatorIdx].updated) 
                accumulators[accumulatorIdx].update(
                    accumulators[accumulatorIdx-1], oppColor(stm), board.lastMove(), board.captured());

            plyData.eval = board.inCheck() ? 0 : evaluate(accumulators[accumulatorIdx], stm);
        }

        if (ply >= maxDepth) return plyData.eval;

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
                u64 hashAfter = board.zobristHash() ^ ZOBRIST_COLOR[(int)stm] ^ ZOBRIST_COLOR[(int)board.oppSide()];
                __builtin_prefetch(probeTT(tt, hashAfter));

                board.makeMove(MOVE_NONE);

                i32 nmpDepth = depth - nmpBaseReduction.value - depth / nmpReductionDivisor.value
                               - std::min((plyData.eval-beta) / nmpEvalBetaDivisor.value, nmpEvalBetaMax.value);

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

        int legalMovesSeen = 0;
        i32 bestScore = -INF;
        Move bestMove = MOVE_NONE;
        Bound bound = Bound::UPPER;

        // Fail low quiets at beginning of array, fail low noisy moves at the end
        std::array<HistoryEntry*, 256> failLowsHistoryEntry;
        int failLowQuiets = 0, failLowNoisies = 0;

        u64 pinned = board.pinned();

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = plyData.moves.incrementalSort(plyData.movesScores, i);

            // Don't search TT move in singular search
            if (move == ttMove && singular) continue;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            legalMovesSeen++;
            bool isQuiet = !board.isCapture(move) && move.promotion() == PieceType::NONE;

            if (bestScore > -MIN_MATE_SCORE && !pvNode && !board.inCheck() && moveScore < COUNTERMOVE_SCORE)
            {
                // LMP (Late move pruning)
                if (legalMovesSeen >= lmpMinMoves.value + depth * depth * lmpDepthMultiplier.value)
                    break;

                i32 lmrDepth = std::max(0, depth - (i32)LMR_TABLE[depth][legalMovesSeen]);

                // FP (Futility pruning)
                if (lmrDepth <= fpMaxDepth.value 
                && alpha < MIN_MATE_SCORE
                && alpha > plyData.eval + fpBase.value + lmrDepth * fpMultiplier.value)
                    break;

                // SEE pruning
                if (depth <= seePruningMaxDepth.value) {
                    i32 threshold = isQuiet ? depth * seeQuietThreshold.value 
                                            : depth * depth * seeNoisyThreshold.value;

                    if (!SEE(board, move, threshold)) continue;
                }
            }

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

                i32 singularBeta = std::max(-INF, (i32)ttEntry->score - i32(depth * singularBetaMultiplier.value));

                i32 singularScore = search((depth - 1) / 2, ply, singularBeta - 1, singularBeta, 
                                           doubleExtsLeft, true);

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
                // Negative extension
                else if (ttEntry->score >= beta)
                    // some other move is probably better than TT move, so reduce TT move search by 2 plies
                    extension = -2;
            }
            // Check extension if no singular extensions
            else if (board.inCheck())
                extension = 1;

            skipExtensions:

            if (!move.isSpecial()) __builtin_prefetch(probeTT(tt, board.zobristHashAfter(move)));

            board.makeMove(move);
            u64 nodesBefore = nodes;
            nodes++;

            accumulatorIdx++;
            accumulators[accumulatorIdx].updated = false;

            int pt = (int)move.pieceType();
            HistoryEntry *historyEntry = &historyTable[(int)stm][pt][move.to()];

    	    // PVS (Principal variation search)

            i32 score = 0, lmr = 0;
            if (legalMovesSeen == 1)
            {
                score = -search(depth - 1 + extension, ply + 1, -beta, -alpha, doubleExtsLeft);
                goto moveSearched;
            }

            // LMR (Late move reductions)
            if (depth >= 3 && moveScore < COUNTERMOVE_SCORE)
            {
                lmr = LMR_TABLE[depth][legalMovesSeen];
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
                Move lastMove;
                if ((lastMove = board.lastMove()) != MOVE_NONE)
                    countermoves[(int)stm][(int)lastMove.pieceType()][lastMove.to()] = move;

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
            ttEntry->update(board.zobristHash(), depth, ply, bestScore, bestMove, bound);

        return bestScore;
    }

    // Quiescence search
    inline i32 qSearch(u8 ply, i32 alpha, i32 beta)
    {
        assert(ply > 0);

        if (isHardTimeUp()) return 0;

        // Update seldepth
        if (ply > maxPlyReached) maxPlyReached = ply;

        if (board.isDraw()) return 0;

        // Probe TT
        TTEntry *ttEntry = probeTT(tt, board.zobristHash());
        bool ttHit = ttEntry->zobristHash == board.zobristHash();

        // TT cutoff
        if (ttHit
        && (ttEntry->getBound() == Bound::EXACT
        || (ttEntry->getBound() == Bound::LOWER && ttEntry->score >= beta) 
        || (ttEntry->getBound() == Bound::UPPER && ttEntry->score <= alpha)))
            return ttEntry->adjustedScore(ply);

        PlyData &plyData = pliesData[ply];

        if (!accumulators[accumulatorIdx].updated) 
            accumulators[accumulatorIdx].update(
                accumulators[accumulatorIdx-1], board.oppSide(), board.lastMove(), board.captured());

        if (board.inCheck())
            plyData.eval = 0;
        else {
            plyData.eval = evaluate(accumulators[accumulatorIdx], board.sideToMove());

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

        u64 pinned = board.pinned();

        for (int i = 0; i < plyData.moves.size(); i++)
        {
            auto [move, moveScore] = plyData.moves.incrementalSort(plyData.movesScores, i);

            // SEE pruning (skip bad noisy moves)
            if (!board.inCheck() && moveScore < 0) break;

            // skip illegal moves
            if (!board.isPseudolegalLegal(move, pinned)) continue;

            if (!move.isSpecial()) __builtin_prefetch(probeTT(tt, board.zobristHashAfter(move)));

            board.makeMove(move);
            legalMovesPlayed++;
            nodes++;

            accumulatorIdx++;
            accumulators[accumulatorIdx].updated = false;

            i32 score = -qSearch(ply + 1, -beta, -alpha);

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
        Move lastMove = board.lastMove();

        Move countermove = lastMove == MOVE_NONE
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