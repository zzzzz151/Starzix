# Starzix - C++ chess engine

# Elo (4.0)

[CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3555 (#32/733)

[CCRL Rapid](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3479 (#30/585)

[Ipman Bullet](https://ipmanchess.yolasite.com/r9-7945hx.php): 3334 (#48/51)

# How to compile

### Windows

```clang++ -std=c++20 -march=native -O3 -DNDEBUG -Wl,/STACK:16777216 src/main.cpp -o Starzix.exe```

### Linux

```clang++ -std=c++20 -lstdc++ -lm -march=native -O3 -DNDEBUG src/main.cpp -o starzix```

# UCI (Universal Chess Interface)

### Options

- Hash (int, default 32, 1 to 65536) - transposition table size in MB

- Threads (int, default 1, 1 to 256) - search threads

### Extra commands

- display

- eval

- perft \<depth\> 

- perftsplit \<depth\>

- bench \<depth\>

- makemove \<move\>

- undomove

# Features

### Board
- Bitboards
- Zobrist hashing
- Pseudolegal move generation (magic bitboards for sliders, lookup tables for pawns, knights and king)
- Copymake make/undo move

### NNUE evaluation 
- (768 -> 1024)x2 -> 1
- Lc0 data
- Trained with [my trainer](https://github.com/zzzzz151/nn-trainer)

### Search
- Iterative deepening
- Fail-soft Negamax
- Principal variation search
- Aspiration windows
- Quiescence search
- Transposition table
- Alpha-beta pruning
- Reverse futility pruning
- Razoring
- Null move pruning
- Late move pruning
- Futility pruning
- SEE pruning
- Internal iterative reduction
- Late move reductions
- Singular extensions (with negative and double extensions)
- Check extension
- Multithreading / Lazy SMP

### Move ordering
- TT move
- Good noisy moves by SEE + MVV + noisy history
- Killer move
- Countermove
- Quiet moves by history
- Bad noisy moves

### Moves history
- Main history
- Continuation histories (1 ply, 2 ply, 4 ply)
- Noisy history
- History malus and gravity

### Time management
- Any time control
- Soft and hard limits
- Nodes TM
