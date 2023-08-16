# z5 is a UCI C++ chess engine

# How to compile

**Linux**

```make``` to create the 'z5' binary

**Windows**

```g++ -O3 -std=c++17 src/main.cpp -o z5.exe```

# Features

PeSTO's tapered evaluation with piece values and PSTs

Iterative deepening

Negamax with alpha beta pruning and PVS

Quiescence search

Transposition table

Move ordering with SEE and MVVLVA

Aspiration window

Check extension

Killer moves (2 per ply)

Late move reductions

Null move pruning

Reverse futility pruning

Internal iterative reduction

Time management

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Disservin's move gen (chess-library)](https://github.com/Disservin/chess-library)

[Cutechess](https://github.com/cutechess/cutechess)

[Engine Programming Discord](https://discord.gg/pcjr9eXK)

