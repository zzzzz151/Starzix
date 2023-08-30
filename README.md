# z5 is a UCI C++ chess engine

Currently ~2400 elo (tested against [Barbarossa](https://github.com/nionita/Barbarossa) 0.6).

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

Move ordering (hash move > SEE + MVVLVA > promotions > killer moves > history heuristic)

Aspiration window

Check extension

Late move reductions

Null move pruning

Reverse futility pruning

Internal iterative reduction

Time management

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Disservin's move gen (chess-library)](https://github.com/Disservin/chess-library)

[MantaRay for C++ neural network inference](https://github.com/TheBlackPlague/MantaRay)

[Cutechess](https://github.com/cutechess/cutechess)

[Engine Programming Discord](https://discord.gg/pcjr9eXK)

