#pragma once

// clang-format off

#include <chrono>

class TimeManager
{
    constexpr static u32 OVERHEAD_MILLISECONDS = 10;
    constexpr static double MOVESTOGO_HARD_TIME_PERCENTAGE = 0.75,
                            MOVESTOGO_SOFT_TIME_PERCENTAGE = 0.5,
                            SUDDEN_DEATH_HARD_TIME_PERCENTAGE = 1.0 / 4.0,
                            SUDDEN_DEATH_SOFT_TIME_PERCENTAGE = 1.0 / 30.0,
                            SOFT_TIME_SCALE_BASE = 0.25,
                            SOFT_TIME_SCALE_MULTIPLIER = 1.75;

    private:

    std::chrono::time_point<std::chrono::steady_clock> start;
    u64 softMilliseconds, hardMilliseconds;
    bool hardTimeUp;
    bool isMoveTime, isNodesTime;
    u64 softNodes, hardNodes;

    public:

    inline TimeManager(i64 milliseconds = -1, i64 incrementMilliseconds = 0, i64 movesToGo = -1, bool isMoveTime = false)
    {
        start = std::chrono::steady_clock::now();

        hardTimeUp = false;
        this->isMoveTime = isMoveTime;
        isNodesTime = false;

        if (isMoveTime)
            softMilliseconds = hardMilliseconds = milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2); 
        else if (movesToGo != -1)
        {
            hardMilliseconds = movesToGo == 1 
                               ? milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2)
                               : milliseconds * MOVESTOGO_HARD_TIME_PERCENTAGE;

            softMilliseconds = min(MOVESTOGO_SOFT_TIME_PERCENTAGE * milliseconds / movesToGo, (double)hardMilliseconds);
        }
        else if (milliseconds != -1)
        {
            // "go wtime 10000 btime 10000 winc 100 binc 100"
            hardMilliseconds = milliseconds * SUDDEN_DEATH_HARD_TIME_PERCENTAGE;
            softMilliseconds = milliseconds * SUDDEN_DEATH_SOFT_TIME_PERCENTAGE;
        }
        else
        {
            // received just 'go' or 'go infinite'
            this->isMoveTime = true;
            softMilliseconds = hardMilliseconds = U64_MAX;
        }
    }

    inline TimeManager(u64 softNodes, u64 hardNodes)
    {
        start = std::chrono::steady_clock::now();
        this->isNodesTime = true;
        this->softNodes = softNodes;
        this->hardNodes = hardNodes;
    }

    inline auto millisecondsElapsed()
    {
        return (std::chrono::steady_clock::now() - start) / std::chrono::milliseconds(1);
    }

    inline bool isHardTimeUp(u64 nodes)
    {
        if (isNodesTime) return nodes >= hardNodes;

        if (hardTimeUp) return true;

        // Check time every 1024 nodes
        if ((nodes % 1024) != 0) return false;

        return hardTimeUp = (millisecondsElapsed() >= hardMilliseconds);
    }

    inline bool isSoftTimeUp(u64 nodes, u64 bestMoveNodes)
    {
        if (isNodesTime) return nodes >= softNodes;

        if (isMoveTime) return millisecondsElapsed() >= softMilliseconds;

        double bestMoveNodesFraction = (double)bestMoveNodes / (double)nodes;
        double softTimeScale = (SOFT_TIME_SCALE_BASE + 1 - bestMoveNodesFraction) * SOFT_TIME_SCALE_MULTIPLIER;
        return millisecondsElapsed() >= (softMilliseconds * softTimeScale);
    }

};