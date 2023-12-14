#pragma once

// clang-format off

#include <chrono>

class TimeManager
{
    constexpr static u32 OVERHEAD_MILLISECONDS = 10;

    private:

    std::chrono::time_point<std::chrono::steady_clock> start;
    u64 softMilliseconds, hardMilliseconds;
    bool hardTimeUp;

    public:
    
    u64 softNodes, hardNodes;

    inline TimeManager(i64 milliseconds = -1, i64 incrementMilliseconds = 0, i64 movesToGo = -1, 
                       i64 isMoveTime = false, u64 softNodes = U64_MAX, u64 hardNodes = U64_MAX)
    {
        start = std::chrono::steady_clock::now();
        this->softNodes = softNodes;
        this->hardNodes = hardNodes;
        hardTimeUp = false;

        if (isMoveTime)
            softMilliseconds = hardMilliseconds = milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2); 
        else if (movesToGo != -1)
        {
            hardMilliseconds = movesToGo == 1 
                               ? milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2)
                               : milliseconds * search::movesToGoHardTimePercentage.value;

            softMilliseconds = min(search::movesToGoSoftTimePercentage.value * milliseconds / movesToGo, 
                                   (double)hardMilliseconds);
        }
        else if (milliseconds != -1)
        {
            // "go wtime 10000 btime 10000 winc 100 binc 100"
            hardMilliseconds = milliseconds * search::suddenDeathHardTimePercentage.value;
            softMilliseconds = milliseconds * search::suddenDeathSoftTimePercentage.value;
        }
        else
            // received just 'go' or 'go infinite'
            softMilliseconds = hardMilliseconds = U64_MAX;
    }

    inline void restart() {
        start = std::chrono::steady_clock::now();
        hardTimeUp = false;
    }

    inline auto millisecondsElapsed() {
        return (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
    }

    inline bool isHardTimeUp(u64 nodes)
    {
        if (hardTimeUp || nodes >= hardNodes) return true;

        // Check time every 1024 nodes
        if ((nodes % 1024) != 0) return false;

        return hardTimeUp = (millisecondsElapsed() >= hardMilliseconds);
    }

    inline bool isSoftTimeUp(u64 nodes, u64 bestMoveNodes)
    {
        if (nodes >= softNodes) return true;

        double bestMoveNodesFraction = (double)bestMoveNodes / (double)nodes;
        double softTimeScale = (search::softTimeScaleBase.value + 1 - bestMoveNodesFraction) 
                               * search::softTimeScaleMultiplier.value;
        return millisecondsElapsed() >= (softMilliseconds * softTimeScale);
    }

};