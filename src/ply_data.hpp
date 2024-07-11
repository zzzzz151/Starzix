// clang-format off

#pragma once

enum class MoveGenType : i8 {
    NONE = -1, ALL = 0, NOISIES_ONLY = 1
};

struct PlyData {
    public:

    MoveGenType mMovesGenerated = MoveGenType::NONE;
    ArrayVec<Move, 256> mMoves;
    std::array<i32, 256> mMovesScores;
    int mCurrentMoveIdx = -1;

    ArrayVec<Move, MAX_DEPTH+1> mPvLine = {};
    i32 mEval = EVAL_NONE;
    Move mKiller = MOVE_NONE;
    u64 mEnemyAttacks = 0;

    inline void genAndScoreMoves(Board &board, bool noisiesOnly, Move ttMove, MultiArray<HistoryEntry, 2, 6, 64> &movesHistory) 
    {
        // Generate enemy attacks if not already generated
        if (mMovesGenerated == MoveGenType::NONE)
            mEnemyAttacks = board.attacks(board.oppSide());

        // Generate moves if not already generated
        // Never generate underpromotions in search
        if (mMovesGenerated != (MoveGenType)noisiesOnly) {
            board.pseudolegalMoves(mMoves, noisiesOnly, false);
            mMovesGenerated = (MoveGenType)noisiesOnly;
        }

        // Score moves

        int stm = (int)board.sideToMove();

        for (size_t i = 0; i < mMoves.size(); i++)
        {
            Move move = mMoves[i];

            if (move == ttMove) {
                mMovesScores[i] = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();

            // Starzix doesnt generate underpromotions in search
            assert(promotion == PieceType::NONE || promotion == PieceType::QUEEN);

            if (captured != PieceType::NONE)
            {
                mMovesScores[i] = SEE(board, move) 
                                  ? (promotion == PieceType::QUEEN
                                     ? GOOD_QUEEN_PROMO_SCORE 
                                     : GOOD_NOISY_SCORE)
                                  : -GOOD_NOISY_SCORE;

                // MVV (most valuable victim)
                mMovesScores[i] += 100 * (i32)captured - (i32)move.pieceType();
            }
            else if (promotion == PieceType::QUEEN)
                mMovesScores[i] = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE : -GOOD_NOISY_SCORE;
            else if (move == mKiller)
                mMovesScores[i] = KILLER_SCORE;
            else {
                int pt = (int)move.pieceType();
                
                mMovesScores[i] = movesHistory[stm][pt][move.to()].total(
                    mEnemyAttacks & bitboard(move.from()), 
                    mEnemyAttacks & bitboard(move.to()), 
                    { board.lastMove(), board.nthToLastMove(2) });
            }
        }

        mCurrentMoveIdx = -1; // prepare for moves loop
    }

    inline std::pair<Move, i32> nextMove()
    {
        mCurrentMoveIdx++;

        if (mCurrentMoveIdx >= (int)mMoves.size())
            return { MOVE_NONE, 0 };

        // Incremental sort
        for (size_t j = mCurrentMoveIdx + 1; j < mMoves.size(); j++)
            if (mMovesScores[j] > mMovesScores[mCurrentMoveIdx])
            {
                mMoves.swap(mCurrentMoveIdx, j);
                std::swap(mMovesScores[mCurrentMoveIdx], mMovesScores[j]);
            }

        return { mMoves[mCurrentMoveIdx], mMovesScores[mCurrentMoveIdx] };
    }

    inline void updatePV(Move move) 
    {
        // This function is called from SearchThread in search.cpp, which has a
        // std::array<PlyData, MAX_DEPTH+1> mPliesData
        // so the data is continuous and we can fetch the next ply data
        // by incrementing this pointer
        PlyData* nextPlyData = this + 1;

        mPvLine.clear();
        mPvLine.push_back(move);

        // Copy child's PV
        for (Move move : nextPlyData->mPvLine)
            mPvLine.push_back(move);
    }

    inline std::string pvString() 
    {
        std::string str = "";
        for (Move move : mPvLine)
            str += move.toUci() + " ";

        trim(str);
        return str;
    }
};