# z5 - UCI C++ chess engine

# How to build

```g++ -O3 -std=c++17 src/main.cpp -o z5.exe```

# UCI options

Hash (int, default 64) - transposition table size in MB from 1 to 512

# Features

NNUE evaluation (768->128x2->1)

Negamax with alpha beta pruning and PVS

Iterative deepening

Transposition table

Quiescence search with SEE pruning

Move ordering (hash move > good captures by SEE + MVVLVA > promotions > killer moves > countermove > history heuristic > bad captures)

Aspiration windows

Check and 7th-rank-pawn extensions

Internal iterative reduction

Late move reductions

Null move pruning

Reverse futility pruning

Futility pruning

Late move pruning

SEE depth-based pruning

Time management

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Cutechess](https://github.com/cutechess/cutechess) for engine testing

[MantaRay](https://github.com/TheBlackPlague/MantaRay) for C++ neural network inference
