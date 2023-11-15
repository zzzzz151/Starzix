# z5 - C++ chess engine

# How to compile

**Windows**

```clang++ -O3 -march=native -std=c++20 src/main.cpp -o z5.exe```

**Linux**

```clang++ -O3 -march=native -std=c++20 src/main.cpp -o z5```

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

Move ordering (TT move > good captures by SEE + MVV + history > promotions > killer move > countermove > quiet moves by history > bad captures)

History (main history, countermove history, follow-up move history, capture history)

Aspiration windows

Check extension

Singular extensions (double extensions, negative extension)

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
