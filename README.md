# z5 - UCI C++ chess engine

Currently ~2800 elo (tested against [4ku](https://github.com/kz04px/4ku) 3.0).

# How to compile

**Linux**

```make``` to create the 'z5' binary

**Windows**

```g++ -O3 -std=c++17 src/main.cpp -o z5.exe```

# UCI options

Hash (int, default 64) - transposition table size in MB from 1 to 512

Info (bool, default false) - print info for every iterative deepening iteration

# Features

NNUE evaluation (768->128x2->1)

Negamax with alpha beta pruning and PVS

Iterative deepening

Quiescence search

Transposition table

Move ordering (hash move > good captures by SEE + MVVLVA > promotions > killer moves > history heuristic > bad captures)

Aspiration window

Check extension

Late move reductions

Null move pruning

Futility pruning

Reverse futility pruning

Internal iterative reduction

Time management

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Cutechess for testing](https://github.com/cutechess/cutechess)

[Disservin's move gen (chess-library)](https://github.com/Disservin/chess-library)

[MantaRay for C++ neural network inference](https://github.com/TheBlackPlague/MantaRay)

[Engine Programming Discord](https://discord.gg/pcjr9eXK)

