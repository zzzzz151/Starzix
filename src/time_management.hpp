#pragma once

// clang-format off

#include <chrono>

const i32 OVERHEAD_MILLISECONDS = 10;
const double MOVESTOGO_HARD_TIME_PERCENTAGE = 0.75,
             MOVESTOGO_SOFT_TIME_PERCENTAGE = 0.5,
             SUDDEN_DEATH_HARD_TIME_PERCENTAGE = 1.0 / 4.0,
             SUDDEN_DEATH_SOFT_TIME_PERCENTAGE = 1.0 / 30.0,
             SOFT_TIME_SCALE_BASE = 0.25,
             SOFT_TIME_SCALE_MULTIPLIER = 1.75;

chrono::steady_clock::time_point start;
bool isMoveTime = false;
i32 hardMilliseconds, softMilliseconds;
bool hardTimeUp = false;

inline void setupTime(vector<string> &tokens, Color colorToMove)
{
    start = chrono::steady_clock::now();
    isMoveTime = hardTimeUp = false;
    i32 milliseconds = -1, movesToGo = -1, increment = 0;

    for (int i = 0; i < tokens.size(); i++)
    {
        if ((tokens[i] == "wtime" && colorToMove == Color::WHITE) 
        ||  (tokens[i] == "btime" && colorToMove == Color::BLACK))
            milliseconds = stoi(tokens[i + 1]);
        else if ((tokens[i] == "winc" && colorToMove == Color::WHITE) 
        ||       (tokens[i] == "binc" && colorToMove == Color::BLACK))
            increment = stoi(tokens[i+1]);
        else if (tokens[i] == "movestogo")
            movesToGo = stoi(tokens[i + 1]);
        else if (tokens[i] == "movetime")
        {
            isMoveTime = true;
            milliseconds = stoi(tokens[i + 1]);
        }
    }

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
        isMoveTime = true;
        softMilliseconds = hardMilliseconds = MAX_INT32;
    }
}

inline bool isHardTimeUp(u64 nodes)
{
    if (hardTimeUp) return true;

    if ((nodes % 1024) != 0) return false;

    return hardTimeUp = (millisecondsElapsed(start) >= hardMilliseconds);
}

inline bool isSoftTimeUp(double bestMoveNodesFraction)
{
    double softTimeScale = isMoveTime ? 1 : (SOFT_TIME_SCALE_BASE + 1 - bestMoveNodesFraction) * SOFT_TIME_SCALE_MULTIPLIER;
    return millisecondsElapsed(start) >= (softMilliseconds * softTimeScale);
}


