// clang-format off

#pragma once

#include <limits>

enum class MoveGenStage : int {
    TT_MOVE = 0, GEN_SCORE_NOISIES, GOOD_NOISIES, KILLER, 
    GEN_SCORE_QUIETS, QUIETS, BAD_NOISIES, END
};

// Incremental sorting with partial selection sort
inline std::pair<Move, i32> partialSelectionSort(
    ArrayVec<Move, 256> &moves, std::array<i32, 256> &movesScores, int &idx)
{
    idx++;

    if (idx >= (int)moves.size())
        return { MOVE_NONE, 0 };

    for (size_t i = idx + 1; i < moves.size(); i++)
        if (movesScores[i] > movesScores[idx])
        {
            moves.swap(i, idx);
            std::swap(movesScores[i], movesScores[idx]);
        }

    return { moves[idx], movesScores[idx] };
}

struct MovePicker {
    private:

    bool mNoisiesOnly;
    Board* mBoard;
    u64 mPinned;
    u64 mEnemyAttacks = 0;
    Move mTtMove;
    Move mKiller;
    MultiArray<HistoryEntry, 2, 6, 64>* mMovesHistory;

    MoveGenStage mStage = MoveGenStage::TT_MOVE;

    ArrayVec<Move, 256> mNoisies, mQuiets;
    std::array<i32, 256> mNoisiesScores, mQuietsScores;
    int mNoisiesIdx = 0, mQuietsIdx = 0;

    bool mBadNoisyReady = false;

    public:

    inline MovePicker(const bool noisiesOnly, Board* board, const Move ttMove, const Move killer, 
        MultiArray<HistoryEntry, 2, 6, 64>* movesHistory) 
    {
        mNoisiesOnly = noisiesOnly;
        mBoard = board;
        mPinned = mBoard->pinned();
        mTtMove = ttMove;
        mKiller = killer;
        mMovesHistory = movesHistory;
    }

    inline MoveGenStage stage() const { return mStage; }

    inline u64 enemyAttacks() const { return mEnemyAttacks; }

    inline i32 moveScore() const 
    { 
        assert(mStage == MoveGenStage::QUIETS 
            || mStage == MoveGenStage::GOOD_NOISIES 
            || mStage == MoveGenStage::BAD_NOISIES
        );

        return mStage == MoveGenStage::QUIETS ? mQuietsScores[mQuietsIdx] : mNoisiesScores[mNoisiesIdx];
    }

    inline Move next() {
        size_t i;

        switch (mStage) 
        {
        case MoveGenStage::TT_MOVE:
        {
            if (mBoard->isPseudolegal(mTtMove) && mBoard->isPseudolegalLegal(mTtMove, mPinned))
                return mTtMove;

            mStage = MoveGenStage::GEN_SCORE_NOISIES;
            [[fallthrough]];
        }
        case MoveGenStage::GEN_SCORE_NOISIES:
        {
            // Generate pseudolegal noisy moves, except underpromotions
            mBoard->pseudolegalMoves(mNoisies, MoveGenType::NOISIES, false);

            // Score moves
            i = 0;
            while (i < mNoisies.size())
            {
                const Move move = mNoisies[i];

                // Assert move is noisy
                assert(mBoard->captured(move) != PieceType::NONE || move.promotion() != PieceType::NONE);

                // Assert no underpromotions
                assert(move.promotion() == PieceType::NONE || move.promotion() == PieceType::QUEEN);

                // Remove TT move and mKiller move from list
                if (move == mTtMove || move == mKiller)
                {
                    mNoisies.swap(i, mNoisies.size() - 1);
                    mNoisies.pop_back();
                    continue;
                }

                constexpr i32 GOOD_NOISY_SCORE = 1'000'000;

                mNoisiesScores[i] = mNoisiesOnly 
                                    ? (move.promotion() == PieceType::QUEEN ? 1'000'000 : 0)
                                    : (mBoard->SEE(move)
                                       ? (move.promotion() == PieceType::QUEEN ? GOOD_NOISY_SCORE * 2 : GOOD_NOISY_SCORE)
                                       : -GOOD_NOISY_SCORE
                                      );

                // MVVLVA (most valuable victim, least valuable attacker)
                mNoisiesScores[i] += 100 * (i32)mBoard->captured(move) - (i32)move.pieceType();

                i++;
            }

            mStage = MoveGenStage::GOOD_NOISIES;
            [[fallthrough]];
        }
        case MoveGenStage::GOOD_NOISIES:
        {
            while (true) {
                auto [nextMove, moveScore] = partialSelectionSort(mNoisies, mNoisiesScores, mNoisiesIdx);

                if (nextMove == MOVE_NONE) break;

                if (moveScore < 0) {
                    mBadNoisyReady = true;
                    break;
                }

                if (mBoard->isPseudolegalLegal(nextMove, mPinned))
                    return nextMove;
            }

            mStage = mNoisiesOnly ? MoveGenStage::BAD_NOISIES : MoveGenStage::KILLER;
            return next();
        }
        case MoveGenStage::KILLER:
        {
            if (mBoard->isPseudolegal(mKiller) && mBoard->isPseudolegalLegal(mKiller, mPinned))
                return mKiller;

            mStage = MoveGenStage::GEN_SCORE_QUIETS;
            [[fallthrough]];
        }
        case MoveGenStage::GEN_SCORE_QUIETS:
        {
            // Generate pseudolegal quiet moves (underpromotions excluded)
            mBoard->pseudolegalMoves(mQuiets, MoveGenType::QUIETS, false);

            const int stm = int(mBoard->sideToMove());

            assert(mEnemyAttacks == 0);
            mEnemyAttacks = mBoard->attacks(mBoard->oppSide());

            const std::array<Move, 3> lastMoves = { 
                mBoard->lastMove(), mBoard->nthToLastMove(2), mBoard->nthToLastMove(4) 
            };

            // Score moves
            i = 0;
            while (i < mQuiets.size())
            {
                const Move move = mQuiets[i];

                // Assert move is quiet
                assert(mBoard->captured(move) == PieceType::NONE && move.promotion() == PieceType::NONE);

                // Remove TT move and mKiller move from list
                if (move == mTtMove || move == mKiller)
                {
                    mQuiets.swap(i, mQuiets.size() - 1);
                    mQuiets.pop_back();
                    continue;
                }

                const int pt = (int)move.pieceType();
                
                mQuietsScores[i] = (*mMovesHistory)[stm][pt][move.to()].quietHistory(
                    mEnemyAttacks & bitboard(move.from()), 
                    mEnemyAttacks & bitboard(move.to()), 
                    lastMoves);

                i++;
            }

            mStage = MoveGenStage::QUIETS;
            [[fallthrough]];
        }
        case MoveGenStage::QUIETS:
        {
            while (true) {
                auto [nextMove, moveScore] = partialSelectionSort(mQuiets, mQuietsScores, mQuietsIdx);

                if (nextMove == MOVE_NONE) break;

                if (mBoard->isPseudolegalLegal(nextMove, mPinned))
                    return nextMove;
            }

            mStage = MoveGenStage::BAD_NOISIES;
            [[fallthrough]];
        }
        case MoveGenStage::BAD_NOISIES:
        {
            if (mBadNoisyReady) {
                mBadNoisyReady = false;

                if (mBoard->isPseudolegalLegal(mNoisies[mNoisiesIdx], mPinned))
                    return mNoisies[mNoisiesIdx];
            }

            while (true) {
                auto [nextMove, moveScore] = partialSelectionSort(mNoisies, mNoisiesScores, mNoisiesIdx);

                if (nextMove == MOVE_NONE) break;

                assert(moveScore < 0);

                if (mBoard->isPseudolegalLegal(nextMove, mPinned))
                    return nextMove;
            }

            mStage = MoveGenStage::END;
            [[fallthrough]];
        }
        default:
            return MOVE_NONE;
        }
    }
};