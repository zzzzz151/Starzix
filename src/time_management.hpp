#ifndef TIME_MANAGEMENT_HPP
#define TIME_MANAGEMENT_HPP

// clang-format off
using namespace std;

chrono::steady_clock::time_point start;
int millisecondsForThisTurn;
bool isTimeUp = false;

inline void setupTime(vector<string> &words, Color color)
{
    start = chrono::steady_clock::now();
    isTimeUp = false;
    int milliseconds = -1, movesToGo = -1, increment = 0;
    bool isMoveTime = false;

    // With movestogo or movetime, save 5ms for shenanigans

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

    if (isMoveTime || movesToGo == 1)
        // save 5ms for shenanigans
        millisecondsForThisTurn = milliseconds - min(5, milliseconds / 2); 
    else if (movesToGo != -1)
    	millisecondsForThisTurn = milliseconds / movesToGo;
    else
        // if invalid, use 1 min by default, else it's "go wtime 10000 btime 10000 winc 100 binc 100"
        millisecondsForThisTurn = milliseconds == -1 ? 60000 : round(milliseconds / 15.0);

}

inline bool checkIsTimeUp()
{
    if (isTimeUp) return true;
    isTimeUp = (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= millisecondsForThisTurn;
    return isTimeUp;
}

inline int millisecondsLeftForThisTurn()
{
    int millisecondsElapsed = (chrono::steady_clock::now() - start) / chrono::milliseconds(1);
    return millisecondsForThisTurn - millisecondsElapsed;
}

#endif