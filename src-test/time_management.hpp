#ifndef TIME_MANAGEMENT_HPP
#define TIME_MANAGEMENT_HPP

// clang-format off
using namespace std;

chrono::steady_clock::time_point start;
int millisecondsForThisTurn;
bool isTimeUp = false;

inline void setupTime(vector<string> &words, Color color = Color::WHITE)
{
    start = chrono::steady_clock::now();
    isTimeUp = false;
    millisecondsForThisTurn = -1;
    int milliseconds = -1, movesToGo = -1;

    // With movestogo or movetime, save 5ms for shenanigans

    for (int i = 0; i < words.size(); i++)
    {
        if ((words[i] == "wtime" && color == Color::WHITE) || (words[i] == "btime" && color == Color::BLACK))
        {
            milliseconds = stoi(words[i + 1]);
            if (movesToGo != -1)
            {
                millisecondsForThisTurn = milliseconds / movesToGo;
                if (movesToGo == 1) millisecondsForThisTurn -= min(5.0, milliseconds / 2.0);
            }
            else
                millisecondsForThisTurn = milliseconds / 30.0;
        }
        else if (words[i] == "movestogo")
        {
            int movesToGo = stoi(words[i + 1]);
            if (milliseconds != -1) 
            {
                millisecondsForThisTurn = milliseconds / movesToGo;
                if (movesToGo == 1) millisecondsForThisTurn -= min(5.0, milliseconds / 2.0);
            }
        }
        else if (words[i] == "movetime")
        {
            milliseconds = stoi(words[i + 1]);
            millisecondsForThisTurn = milliseconds - min(5.0, milliseconds / 2.0);
            break;
        }
    }

    if (millisecondsForThisTurn == -1) millisecondsForThisTurn = 60000; // default 1 minute (this line should never happen)

}

inline bool checkIsTimeUp()
{
    if (isTimeUp) return true;
    isTimeUp = (chrono::steady_clock::now() - start) / chrono::milliseconds(1) >= millisecondsForThisTurn;
    return isTimeUp;
}

#endif