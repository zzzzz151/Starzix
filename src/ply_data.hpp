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

    inline void genAndScoreMoves(Board &board, bool noisiesOnly) 
    {
        if (mMovesGenerated != (MoveGenType)noisiesOnly) {
            board.pseudolegalMoves(mMoves, noisiesOnly, false);
            mMovesGenerated = (MoveGenType)noisiesOnly;
        }

        // Score moves

        int stm = (int)board.sideToMove();

        for (size_t i = 0; i < mMoves.size(); i++)
        {
            Move move = mMoves[i];
            PieceType captured = board.captured(move);

            mMovesScores[i] = move.promotion() == PieceType::QUEEN ? 1'000'000 : 0;

            if (captured != PieceType::NONE)
            {
                mMovesScores[i] += 100 * (i32)captured - (i32)move.pieceType();
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