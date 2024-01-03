# Starzix - C++ chess engine

# How to compile

### Windows

```clang++ -O3 -march=native -std=c++20 src/main.cpp -o Starzix.exe```

### Linux

```clang++ -O3 -march=native -std=c++20 src/main.cpp -o starzix```

# UCI (Universal Chess Interface)

### Options

- Hash (int, default 64, 1 to 1024) - transposition table size in MB

### Extra commands

- eval - displays current position's evaluation from perspective of side to move

- perft \<depth\> - runs perft from current position

- perftsplit \<depth\> - runs split perft from current position

# Features

### Board
- Bitboards + mailbox
- Zobrist hashing
- Pseudolegal move generation (magic bitboards for sliders, lookup tables for pawns, knights and king)
- Make/undo move

### NNUE evaluation (768->384x2->1)

### Search framework
- Iterative deepening
- Aspiration windows
- Principal variation search with fail-soft Negamax
- Quiescence search
- Transposition table

### Pruning
- Alpha-beta pruning
- Reverse futility pruning
- Alpha pruning
- Razoring
- Null move pruning
- Late move pruning
- Futility pruning
- SEE pruning

### Extensions
- Singular extensions (double extensions, negative extension)
- Check extension
- 7th-rank-pawn extension

### Reductions
- Internal iterative reduction
- Late move reductions

### Move ordering
- TT move
- Good noisy moves by SEE + MVV + noisy history
- Killer move
- Countermove
- Quiet moves by history
- Bad noisy moves

### Move history
- Main history
- Countermove history
- Follow-up move history
- Noisy history

### Time management
- Any time control
- Soft and hard limits
- Soft limit scaling based on best move nodes

# Credits

[Chess Programming Wiki](https://www.chessprogramming.org/)

[Cutechess](https://github.com/cutechess/cutechess) for engine testing
