// clang-format off

#pragma once

struct PlyData {
    public:

    std::array<Move, MAX_DEPTH+1> mPvLine = { MOVE_NONE };
    u8 mPvLength = 0;
    Move mKiller = MOVE_NONE;
    i32 mEval = INF;
    ArrayVec<Move, 256> mMoves;
    std::array<i32, 256> mMovesScores;

    inline std::string pvString() {
        if (mPvLength == 0) return "";

        std::string pvStr = mPvLine[0].toUci();

        for (u8 i = 1; i < mPvLength; i++)
            pvStr += " " + mPvLine[i].toUci();

        return pvStr;
    }

    inline void genAndScoreMoves(Board &board, bool noisiesOnly, Move ttMove, Move countermove, 
                                 MultiArray<HistoryEntry, 2, 6, 64> &historyTable)
    {
        // never generate underpromotions in search
        board.pseudolegalMoves(mMoves, noisiesOnly, false);

        int stm = (int)board.sideToMove();

        std::array<Move, 4> lastMoves = {
            MOVE_NONE, board.nthToLastMove(1), board.nthToLastMove(2), board.nthToLastMove(4)
        };

        for (std::size_t i = 0; i < mMoves.size(); i++)
        {
            Move move = lastMoves[0] = mMoves[i];

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
    }

    inline std::pair<Move, i32> nextMove(std::size_t idx)
    {
        // Incremental sort
        for (std::size_t j = idx + 1; j < mMoves.size(); j++)
            if (mMovesScores[j] > mMovesScores[idx])
            {
                mMoves.swap(idx, j);
                std::swap(mMovesScores[idx], mMovesScores[j]);
            }

        return { mMoves[idx], mMovesScores[idx] };
    }

}; // struct PlyData