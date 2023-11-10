#pragma once

// clang-format off

const int32_t OVERHEAD_MILLISECONDS = 10;
const double MOVESTOGO_HARD_TIME_PERCENTAGE = 0.75,
             MOVESTOGO_SOFT_TIME_PERCENTAGE = 0.5,
             SUDDEN_DEATH_HARD_TIME_PERCENTAGE = 1.0 / 4.0,
             SUDDEN_DEATH_SOFT_TIME_PERCENTAGE = 1.0 / 30.0,
             SOFT_TIME_SCALE_BASE = 0.25,
             SOFT_TIME_SCALE_MULTIPLIER = 1.75;

chrono::steady_clock::time_point start;
bool isMoveTime = false;
int32_t hardMillisecondsForThisTurn, softMillisecondsForThisTurn;
bool hardTimeUp = false;

inline void setupTime(vector<string> &words, Color colorToMove)
{
    start = chrono::steady_clock::now();
    isMoveTime = hardTimeUp = false;
    int32_t milliseconds = -1, movesToGo = -1, increment = 0;

    for (int i = 0; i < words.size(); i++)
    {
        if ((words[i] == "wtime" && colorToMove == WHITE) || (words[i] == "btime" && colorToMove == BLACK))
            milliseconds = stoi(words[i + 1]);
        else if ((words[i] == "winc" && colorToMove == WHITE) || (words[i] == "binc" && colorToMove == BLACK))
            increment = stoi(words[i+1]);
        else if (words[i] == "movestogo")
            movesToGo = stoi(words[i + 1]);
        else if (words[i] == "movetime")
        {
            isMoveTime = true;
            milliseconds = stoi(words[i + 1]);
        }
    }

    if (isMoveTime)
        softMillisecondsForThisTurn = hardMillisecondsForThisTurn = milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2); 
    else if (movesToGo != -1)
    {
        hardMillisecondsForThisTurn = movesToGo == 1 
                                    ? milliseconds - min(OVERHEAD_MILLISECONDS, milliseconds / 2)
                                    : milliseconds * MOVESTOGO_HARD_TIME_PERCENTAGE;

        softMillisecondsForThisTurn = min(MOVESTOGO_SOFT_TIME_PERCENTAGE * milliseconds / movesToGo, (double)hardMillisecondsForThisTurn);
    }
    else if (milliseconds != -1)
    {
        // "go wtime 10000 btime 10000 winc 100 binc 100"
        hardMillisecondsForThisTurn = milliseconds * SUDDEN_DEATH_HARD_TIME_PERCENTAGE;
        softMillisecondsForThisTurn = milliseconds * SUDDEN_DEATH_SOFT_TIME_PERCENTAGE;
    }
    else
    {
        // received just 'go' or 'go infinite'
        isMoveTime = true;
        softMillisecondsForThisTurn = hardMillisecondsForThisTurn = MAX_INT32;
    }
}

inline bool isHardTimeUp()
{
    if (hardTimeUp) 
        return true;
    return (hardTimeUp = (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= hardMillisecondsForThisTurn);
}

inline bool isSoftTimeUp(double bestMoveNodesFraction)
{
    if (isMoveTime) 
        return isHardTimeUp();

    double softTimeScale = (SOFT_TIME_SCALE_BASE + 1 - bestMoveNodesFraction) * SOFT_TIME_SCALE_MULTIPLIER;
    return (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= softMillisecondsForThisTurn * softTimeScale;
}


