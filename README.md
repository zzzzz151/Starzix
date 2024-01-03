# Starzix - C++ chess engine

# How to compile

```clang++ -O3 -march=native -std=c++20 src/main.cpp -o Starzix.exe```

# UCI (Universal Chess Interface)

**Options**

- Hash (int, default 64, 1 to 1024) - transposition table size in MB

**Extra commands**

- perft \<depth\> - runs perft from current position

- eval - displays current position's evaluation from perspective of side to move

# Features

Bitboards

Pseudolegal move generation (magic bitboards for sliders, lookup tables for pawns, knights and king)

NNUE evaluation (768->384x2->1)

Principal variation search with Negamax

Alpha beta pruning

Iterative deepening

Transposition table

Quiescence search with SEE pruning

Move ordering (hash move > good captures by SEE + MVVLVA > promotions > killer moves > countermove > quiet moves by history > bad captures)

Aspiration windows

Check extension

Internal iterative reduction

Late move reductions

Null move pruning

Reverse futility pruning

Futility pruning

Late move pruning

SEE depth-based pruning

Time management (any time control, soft and hard limits)

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Cutechess](https://github.com/cutechess/cutechess) for engine testing
