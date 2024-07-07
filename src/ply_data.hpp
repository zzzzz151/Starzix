// clang-format off

#pragma once

enum class MoveGenType : i8 {
    NONE = -1, ALL = 0, NOISIES_ONLY = 1
};

struct PlyData {
    public:

    MoveGenType mMovesGenerated = MoveGenType::NONE;
    ArrayVec<Move, 256> mMoves;

    ArrayVec<Move, MAX_DEPTH+1> mPvLine = {};

    inline void genAndScoreMoves(Board &board, bool noisiesOnly) 
    {
        if (mMovesGenerated != (MoveGenType)noisiesOnly) {
            board.pseudolegalMoves(mMoves, noisiesOnly, false);
            mMovesGenerated = (MoveGenType)noisiesOnly;
        }
    }
};