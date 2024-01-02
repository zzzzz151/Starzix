#include "search_params.hpp"
#include "tt.hpp"
#include "see.hpp"
#include "history_entry.hpp"

u8 LMR_TABLE[256][256]; // [depth][moveIndex]

inline void initLmrTable()
{
    memset(LMR_TABLE, 0, sizeof(LMR_TABLE));
    for (int depth = 1; depth < 256; depth++)
        for (int move = 1; move < 256; move++)
            LMR_TABLE[depth][move] = round(lmrBase.value + ln(depth) * ln(move) * lmrMultiplier.value);
}

class Searcher {
    public:

    Board board;
    u8 maxDepth;
    u64 nodes;
    u8 maxPlyReached;
    TT tt;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    bool hardTimeUp;
    u64 softMilliseconds, hardMilliseconds;
    u64 softNodes, hardNodes;
    u64 movesNodes[1ULL << 16];          // [moveEncoded]
    Move pvLines[256][256];              // [ply][ply]
    u8 pvLengths[256];                   // [pvLineIndex]
    Move killerMoves[256];               // [ply]
    Move countermoves[2][1ULL << 16];    // [color][moveEncoded]
    HistoryEntry historyTable[2][6][64]; // [color][pieceType][targetSquare]

    inline Searcher(Board board)
    {
        resetLimits();
        this->board = board;
        nodes = 0;
        maxPlyReached = 0;
        tt = TT(true);
        memset(movesNodes, 0, sizeof(movesNodes));
        memset(pvLines, 0, sizeof(pvLines));
        memset(pvLengths, 0, sizeof(pvLengths));
        memset(killerMoves, 0, sizeof(killerMoves));
        memset(countermoves, 0, sizeof(countermoves));
        memset(historyTable, 0, sizeof(historyTable));
    }

    inline Move bestMoveRoot() {
        return pvLines[0][0];
    }

    inline void resetLimits()
    {
        startTime = std::chrono::steady_clock::now();
        maxDepth = 100;
        hardTimeUp = false;
        hardMilliseconds = softMilliseconds
                         = softNodes
                         = hardNodes
                         = U64_MAX;
    }

    inline void setSuddenDeathTime(u64 milliseconds)
    {
        hardMilliseconds = milliseconds * suddenDeathHardTimePercentage.value;
        softMilliseconds = milliseconds * suddenDeathSoftTimePercentage.value;
    }

    inline void setCyclicTime(u64 milliseconds, u16 movesToGo)
    {
        hardMilliseconds = movesToGo == 1 
                           ? milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2)
                           : milliseconds * movesToGoHardTimePercentage.value;

        softMilliseconds = min(movesToGoSoftTimePercentage.value * milliseconds / movesToGo, 
                               (double)hardMilliseconds);
    }

    inline void setMoveTime(u64 milliseconds) {
        softMilliseconds = hardMilliseconds 
                         = milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2); 
    }

    inline bool isHardTimeUp()
    {
        if (hardTimeUp || nodes >= hardNodes) return true;

        // Check time every 1024 nodes
        if ((nodes % 1024) != 0) return false;

        return hardTimeUp = (millisecondsElapsed(startTime) >= hardMilliseconds);
    }

    inline std::pair<Move, i16> search(bool printInfo = true)
    {
        // reset and initialize stuff
        //startTime = std::chrono::steady_clock::now();
        hardTimeUp = false;
        nodes = 0;
        maxPlyReached = 0;
        memset(movesNodes, 0, sizeof(movesNodes));
        memset(pvLines, 0, sizeof(pvLines));
        memset(pvLengths, 0, sizeof(pvLengths));

        MovesList moves = MovesList();
        board.pseudolegalMoves(moves);
        moves.shuffle();
        for (int i = 0; i < moves.size(); i++)
            if (board.makeMove(moves[i]))
                return { moves[i], 0 };

        return { MOVE_NONE, 0 };
    }

    private:

};