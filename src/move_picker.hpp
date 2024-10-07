// clang-format off

#pragma once

enum class MoveGenStage : int {
    TT_MOVE_NEXT = 0, TT_MOVE_YIELDED, GEN_SCORE_NOISIES, GOOD_NOISIES, 
    KILLER_NEXT, KILLER_YIELDED, GEN_SCORE_QUIETS, QUIETS, BAD_NOISIES, END
};

// Incremental sorting with partial selection sort
constexpr Move partialSelectionSort(
    ArrayVec<Move, 256> &moves, std::array<i32, 256> &movesScores, const int idx)
{
    assert(idx < int(moves.size()));

    for (size_t i = idx + 1; i < moves.size(); i++)
        if (movesScores[i] > movesScores[idx])
        {
            moves.swap(i, idx);
            std::swap(movesScores[i], movesScores[idx]);
        }

    return moves[idx];
}

struct MovePicker {
    private:

    MoveGenStage mStage = MoveGenStage::TT_MOVE_NEXT;
    bool mNoisiesOnly;

    ArrayVec<Move, 256> mNoisies, mQuiets;
    std::array<i32, 256> mNoisiesScores, mQuietsScores;
    int mNoisiesIdx = -1, mQuietsIdx = -1;

    bool mBadNoisyReady = false;

    public:

    constexpr MovePicker(const bool noisiesOnly) {
        mNoisiesOnly = noisiesOnly;
    }

    constexpr MoveGenStage stage() const { return mStage; }

    constexpr i32 moveScore() const 
    { 
        assert(mStage == MoveGenStage::QUIETS 
            || mStage == MoveGenStage::GOOD_NOISIES 
            || mStage == MoveGenStage::BAD_NOISIES
        );

        return mStage == MoveGenStage::QUIETS ? mQuietsScores[mQuietsIdx] : mNoisiesScores[mNoisiesIdx];
    }

    constexpr Move next(Board &board, const Move ttMove, const Move killer, 
        const MultiArray<HistoryEntry, 2, 6, 64> &movesHistory, Move excludedMove = MOVE_NONE) 
    {
        assert(killer == MOVE_NONE || board.isQuiet(killer));

        Move move = MOVE_NONE;
        const int stm = int(board.sideToMove());

        switch (mStage) 
        {
        case MoveGenStage::TT_MOVE_NEXT:
        {
            if (ttMove != excludedMove
            && !(mNoisiesOnly && ttMove != MOVE_NONE && board.isQuiet(ttMove)) 
            && board.isPseudolegal(ttMove) 
            && board.isPseudolegalLegal(ttMove))
            {
                mStage = MoveGenStage::TT_MOVE_YIELDED;
                return ttMove;
            }

            mStage = MoveGenStage::GEN_SCORE_NOISIES;
            break;
        }
        case MoveGenStage::TT_MOVE_YIELDED:
        {
            mStage = MoveGenStage::GEN_SCORE_NOISIES;
            break;
        }
        case MoveGenStage::GEN_SCORE_NOISIES:
        {
            // Generate pseudolegal noisy moves, except underpromotions
            board.pseudolegalMoves(mNoisies, MoveGenType::NOISIES, false);

            // Score moves
            size_t i = 0;
            while (i < mNoisies.size())
            {
                move = mNoisies[i];
                assert(!board.isQuiet(move));

                // Assert no underpromotions
                assert(move.promotion() == PieceType::NONE || move.promotion() == PieceType::QUEEN);

                // Remove TT move and excluded move from list
                if (move == ttMove || move == excludedMove) {
                    mNoisies.swap(i, mNoisies.size() - 1);
                    mNoisies.pop_back();
                    continue;
                }

                constexpr i32 GOOD_NOISY_SCORE = 1'000'000;
                const PieceType captured = board.captured(move);

                if (mNoisiesOnly)
                    mNoisiesScores[i] = move.promotion() == PieceType::QUEEN ? 1'000'000 : 0;
                else if (move.promotion() == PieceType::QUEEN)
                    mNoisiesScores[i] = board.SEE(move, 0) ? GOOD_NOISY_SCORE * 2 : -GOOD_NOISY_SCORE;
                else {
                    const int pt = int(move.pieceType());
                    const i32 noisyHist = movesHistory[stm][pt][move.to()].noisyHistory(captured);

                    mNoisiesScores[i] = board.SEE(move, -noisyHist / seeNoisyHistDiv()) ? GOOD_NOISY_SCORE : -GOOD_NOISY_SCORE;
                }

                // MVVLVA (most valuable victim, least valuable attacker)
                mNoisiesScores[i] += 100 * (i32)captured - i32(move.pieceType());

                i++;
            }

            mStage = MoveGenStage::GOOD_NOISIES;
            break;
        }
        case MoveGenStage::GOOD_NOISIES:
        {
            move = MOVE_NONE;

            while (++mNoisiesIdx < int(mNoisies.size())) 
            {
                move = partialSelectionSort(mNoisies, mNoisiesScores, mNoisiesIdx);

                if (moveScore() < 0) {
                    mBadNoisyReady = true;
                    break;
                }

                if (board.isPseudolegalLegal(move))
                    return move;
            }

            mStage = mNoisiesOnly ? MoveGenStage::BAD_NOISIES : MoveGenStage::KILLER_NEXT;
            break;
        }
        case MoveGenStage::KILLER_NEXT:
        {
            if (killer != excludedMove && board.isPseudolegal(killer) && board.isPseudolegalLegal(killer))
            {
                mStage = MoveGenStage::KILLER_YIELDED;
                return killer;
            }

            mStage = MoveGenStage::GEN_SCORE_QUIETS;
            break;
        }
        case MoveGenStage::KILLER_YIELDED:
        {
            mStage = MoveGenStage::GEN_SCORE_QUIETS;
            break;
        }
        case MoveGenStage::GEN_SCORE_QUIETS:
        {
            // Generate pseudolegal quiet moves (underpromotions excluded)
            board.pseudolegalMoves(mQuiets, MoveGenType::QUIETS, false);

            const Color nstm = board.oppSide();

            const std::array<Move, 3> lastMoves = { 
                board.lastMove(), board.nthToLastMove(2), board.nthToLastMove(4) 
            };

            // Score moves
            size_t j = 0;
            while (j < mQuiets.size())
            {
                move = mQuiets[j];
                assert(board.isQuiet(move));

                // Remove TT move and killer move and excluded move from list
                if (move == ttMove || move == killer || move == excludedMove)
                {
                    mQuiets.swap(j, mQuiets.size() - 1);
                    mQuiets.pop_back();
                    continue;
                }

                const int pt = int(move.pieceType());

                // Calling attacks(board.oppSide()) will cache enemy attacks and speedup isSquareAttacked()
                board.attacks(board.oppSide()); 
                
                mQuietsScores[j] = movesHistory[stm][pt][move.to()].quietHistory(
                    board.isSquareAttacked(move.from(), nstm), 
                    board.isSquareAttacked(move.to(),   nstm), 
                    lastMoves
                );

                j++;
            }

            mStage = MoveGenStage::QUIETS;
            break;
        }
        case MoveGenStage::QUIETS:
        {
            move = MOVE_NONE;

            while (++mQuietsIdx < int(mQuiets.size())) 
            {
                move = partialSelectionSort(mQuiets, mQuietsScores, mQuietsIdx);

                if (board.isPseudolegalLegal(move))
                    return move;
            }

            mStage = MoveGenStage::BAD_NOISIES;
            break;
        }
        case MoveGenStage::BAD_NOISIES:
        {
            if (mBadNoisyReady) {
                assert(moveScore() < 0);
                mBadNoisyReady = false;

                if (board.isPseudolegalLegal(mNoisies[mNoisiesIdx]))
                    return mNoisies[mNoisiesIdx];
            }

            move = MOVE_NONE;

            while (++mNoisiesIdx < int(mNoisies.size())) 
            {
                move = partialSelectionSort(mNoisies, mNoisiesScores, mNoisiesIdx);
                assert(moveScore() < 0);

                if (board.isPseudolegalLegal(move))
                    return move;
            }

            mStage = MoveGenStage::END;
            break;
        }
        default:
            return MOVE_NONE;
        }

        return next(board, ttMove, killer, movesHistory, excludedMove);
    }
};
