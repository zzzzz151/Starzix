#pragma once

// clang-format off

chrono::steady_clock::time_point start;
bool isMoveTime = false;
int softMillisecondsForThisTurn, hardMillisecondsForThisTurn;
bool hardTimeUp = false;

inline void setupTime(vector<string> &words, Color color)
{
    start = chrono::steady_clock::now();
    isMoveTime = hardTimeUp = false;
    int milliseconds = -1, movesToGo = -1, increment = 0;

    for (int i = 0; i < words.size(); i++)
    {
        if ((words[i] == "wtime" && color == WHITE) || (words[i] == "btime" && color == BLACK))
            milliseconds = stoi(words[i + 1]);
        else if ((words[i] == "winc" && color == WHITE) || (words[i] == "binc" && color == BLACK))
            increment = stoi(words[i+1]);
        else if (words[i] == "movestogo")
            movesToGo = stoi(words[i + 1]);
        else if (words[i] == "movetime")
        {
            milliseconds = stoi(words[i + 1]);
            isMoveTime = true;
        }
    }

    if (isMoveTime)
    {
        softMillisecondsForThisTurn = hardMillisecondsForThisTurn = milliseconds - min(10, milliseconds / 2); 
    }
    else if (movesToGo != -1)
    {
        hardMillisecondsForThisTurn = movesToGo == 1 ? milliseconds - min(10, milliseconds / 2) : milliseconds / 2;
        softMillisecondsForThisTurn = min(0.5 * milliseconds / movesToGo, (double)hardMillisecondsForThisTurn);
    }
    else if (milliseconds != -1)
    {
        // "go wtime 10000 btime 10000 winc 100 binc 100"
        hardMillisecondsForThisTurn = milliseconds / 4;
        softMillisecondsForThisTurn = milliseconds / 30;
    }
    else
    {
        // if invalid 'go' command received, default to 1 minute (this should never happen)
        isMoveTime = true;
        softMillisecondsForThisTurn = hardMillisecondsForThisTurn = 60000;
    }
}

inline bool isHardTimeUp()
{
    if (hardTimeUp) 
        return true;
    hardTimeUp = (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= hardMillisecondsForThisTurn;
    return hardTimeUp;
}

inline bool isSoftTimeUp()
{
    if (isMoveTime) 
        return isHardTimeUp();
    return (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= softMillisecondsForThisTurn;
}

