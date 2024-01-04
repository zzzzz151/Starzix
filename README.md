# Starzix - C++ chess engine

# Elo (version 3.0)

[CCRL Blitz](https://computerchess.org.uk/ccrl/404/): 3488

[CCRL Rapid](https://computerchess.org.uk/ccrl/4040/): 3335

[Ipman Bullet](https://ipmanchess.yolasite.com/r9-7945hx.php): 3309

# How to compile

### Windows

```clang++ -std=c++20 -march=native -O3 -Wl,/STACK:16777216 src/main.cpp -o Starzix.exe```

### Linux

```clang++ -std=c++20 -march=native -O3 src/main.cpp -o starzix```

# UCI (Universal Chess Interface)

### Options

- Hash (int, default 32, 1 to 1024) - transposition table size in MB

### Extra commands

- eval - displays current position's evaluation from perspective of side to move

- perft \<depth\> - run perft from current position

- perftsplit \<depth\> - run split perft from current position

- bench \<depth\> - run benchmark, default depth 14

# Features

### Board
- Bitboards
- Zobrist hashing
- Pseudolegal move generation (magic bitboards for sliders, lookup tables for pawns, knights and king)
- Copymake make/undo move

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

