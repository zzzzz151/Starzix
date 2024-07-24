// clang-format off

#pragma once

struct PlyData {
    public:

    // For main search
    ArrayVec<Move, 256> mAllMoves;
    bool mAllMovesGenerated = false;

    // For qsearch
    ArrayVec<Move, 256> mNoisyMoves; 
    bool mNoisyMovesGenerated = false;

    std::array<i32, 256> mMovesScores;
    int mCurrentMoveIdx = -1;

    ArrayVec<Move, MAX_DEPTH+1> mPvLine = {};
    i32 mEval = EVAL_NONE;
    Move mKiller = MOVE_NONE;
    u64 mEnemyAttacks = 0;

    // Soft reset when we make a move in search
    inline void softReset() {
        mAllMovesGenerated = mNoisyMovesGenerated = false;
        mPvLine.clear();
        mEval = EVAL_NONE;
    }

    // For main search
    inline void genAndScoreMoves(Board &board, Move ttMove, Move countermove,
        MultiArray<HistoryEntry, 2, 6, 64> &movesHistory) 
    {
        assert(!(board.lastMove() == MOVE_NONE && countermove != MOVE_NONE));

        // Generate all moves (except underpromotions) and enemy attacks, if not already generated
        if (!mAllMovesGenerated) {
            board.pseudolegalMoves(mAllMoves, false, false);
            mAllMovesGenerated = true;
            mEnemyAttacks = board.attacks(board.oppSide());
        }

        // Score moves
        for (size_t i = 0; i < mAllMoves.size(); i++)
        {
            Move move = mAllMoves[i];

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

                // MVVLVA (most valuable victim, least valuable attacker)
                mMovesScores[i] += 100 * (i32)captured - (i32)move.pieceType();
            }
            else if (promotion == PieceType::QUEEN)
                mMovesScores[i] = SEE(board, move) ? GOOD_QUEEN_PROMO_SCORE : -GOOD_NOISY_SCORE;
            else if (move == mKiller)
                mMovesScores[i] = KILLER_SCORE;
            else if (move == countermove)
                mMovesScores[i] = COUNTERMOVE_SCORE;
            else {
                int stm = (int)board.sideToMove();
                int pt = (int)move.pieceType();
                
                mMovesScores[i] = movesHistory[stm][pt][move.to()].quietHistory(
                    mEnemyAttacks & bitboard(move.from()), 
                    mEnemyAttacks & bitboard(move.to()), 
                    { board.lastMove(), board.nthToLastMove(2) });
            }
        }

        // prepare for nextMove() usage in moves loop
        mCurrentMoveIdx = -1; 
    }

    // For qsearch
    inline void genAndScoreNoisyMoves(Board &board) 
    {
        // Generate noisy moves (except underpromotions) if not already generated
        if (!mNoisyMovesGenerated) {
            board.pseudolegalMoves(mNoisyMoves, true, false);
            mNoisyMovesGenerated = true;
        }

        // Score moves
        for (size_t i = 0; i < mNoisyMoves.size(); i++) 
        {
            Move move = mNoisyMoves[i];
            PieceType captured = board.captured(move);
            PieceType promotion = move.promotion();

            // Starzix doesnt generate underpromotions in search
            assert(promotion == PieceType::NONE || promotion == PieceType::QUEEN);

            // Assert only noisy moves were generated
            assert(captured != PieceType::NONE || promotion == PieceType::QUEEN);

            // MVVLVA (most valuable victim, least valuable attacker)
            mMovesScores[i] = 100 * (i32)captured - (i32)move.pieceType();

            if (promotion == PieceType::QUEEN)
                mMovesScores[i] += 1'000'000;
        }

        // prepare for nextMove() usage in moves loop
        mCurrentMoveIdx = -1;
    }

    inline std::pair<Move, i32> nextMove(ArrayVec<Move, 256> &moves)
    {
        mCurrentMoveIdx++;

        if (mCurrentMoveIdx >= (int)moves.size())
            return { MOVE_NONE, 0 };

        // Incremental sort
        for (size_t j = mCurrentMoveIdx + 1; j < moves.size(); j++)
            if (mMovesScores[j] > mMovesScores[mCurrentMoveIdx])
            {
                moves.swap(mCurrentMoveIdx, j);
                std::swap(mMovesScores[mCurrentMoveIdx], mMovesScores[j]);
            }

        return { moves[mCurrentMoveIdx], mMovesScores[mCurrentMoveIdx] };
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