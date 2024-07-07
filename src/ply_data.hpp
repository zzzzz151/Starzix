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