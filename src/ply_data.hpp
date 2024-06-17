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
    
    ArrayVec<Move, MAX_DEPTH+1> mPvLine;
    Move mKiller = MOVE_NONE;
    i32 mEval = INF;

    inline void genAndScoreMoves(Board &board, bool noisiesOnly, Move ttMove, Move countermove, 
                                 MultiArray<HistoryEntry, 2, 6, 64> &historyTable)
    {
        // Generate moves if not already generated
        // Never generate underpromotions in search
        if (mMovesGenerated != (MoveGenType)noisiesOnly) {
            board.pseudolegalMoves(mMoves, noisiesOnly, false);
            mMovesGenerated = (MoveGenType)noisiesOnly;
        }

        // Score moves

        int stm = (int)board.sideToMove();

        std::array<Move, 3> lastMoves = {
            board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4)
        };

        for (std::size_t i = 0; i < mMoves.size(); i++)
        {
            Move move = mMoves[i];

            if (move == ttMove) {
                mMovesScores[i] = I32_MAX;
                continue;
            }

            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();
            
            int pt = (int)move.pieceType();
            HistoryEntry &historyEntry = historyTable[stm][pt][move.to()];

            if (captured != PieceType::NONE)
            {
                mMovesScores[i] = SEE(board, move) 
                                  ? (promotion == PieceType::QUEEN
                                     ? GOOD_QUEEN_PROMO_SCORE 
                                     : GOOD_NOISY_SCORE)
                                  : -GOOD_NOISY_SCORE;

                // MVV (most valuable victim)
                mMovesScores[i] += ((i32)captured + 1) * 1'000'000; 

                mMovesScores[i] += historyEntry.noisyHistory(captured);
            }
            else if (promotion == PieceType::QUEEN)
            {
                mMovesScores[i] = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE : -GOOD_NOISY_SCORE;
                mMovesScores[i] += historyEntry.noisyHistory(captured);
            }
            else if (move == mKiller)
                mMovesScores[i] = KILLER_SCORE;
            else if (move == countermove)
                mMovesScores[i] = COUNTERMOVE_SCORE;
            else
                mMovesScores[i] = historyEntry.quietHistory(lastMoves);
        }

        mCurrentMoveIdx = -1; // prepare for moves loop
    }
    
    inline std::pair<Move, i32> nextMove()
    {
        mCurrentMoveIdx++;

        if (mCurrentMoveIdx >= (int)mMoves.size())
            return { MOVE_NONE, 0 };

        // Incremental sort
        for (std::size_t j = mCurrentMoveIdx + 1; j < mMoves.size(); j++)
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

    inline std::string pvString() {
        if (mPvLine.size() == 0) return "";

        std::string pvStr = mPvLine[0].toUci();

        for (std::size_t i = 1; i < mPvLine.size(); i++)
            pvStr += " " + mPvLine[i].toUci();

        return pvStr;
    }

}; // struct PlyData