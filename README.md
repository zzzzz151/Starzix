# Starzix - C++ chess engine

# Elo

[CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3553 (#25/712)

[CCRL Rapid](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3472 (#25/571)

[Ipman Bullet](https://ipmanchess.yolasite.com/r9-7945hx.php): 3346 (#38/51)

# How to compile

### Windows

```clang++ -std=c++20 -march=x86-64-v3 -O3 -Wl,/STACK:16777216 src/main.cpp -o Starzix.exe```

### Linux

```clang++ -std=c++20 -march=x86-64-v3 -O3 src/main.cpp -o starzix```

# UCI (Universal Chess Interface)

### Options

- Hash (int, default 32, 1 to 1024) - transposition table size in MB

### Extra commands

- display - display current position, fen and zobrist hash

- eval - display current position's evaluation from perspective of side to move

- perft \<depth\> - run perft from current position

- perftsplit \<depth\> - run split perft from current position

- bench \<depth\> - run benchmark, default depth 14

# Features

### Board
- Bitboards
- Zobrist hashing
- Pseudolegal move generation (magic bitboards for sliders, lookup tables for pawns, knights and king)
- Copymake make/undo move

### NNUE evaluation 
- (768->512)x2->1
- Lc0 data
- SCReLU 181

### Search
- Iterative deepening
- Aspiration windows
- Fail-soft Negamax
- Principal variation search
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

### Move ordering
- TT move
- Good noisy moves by SEE + MVV + noisy history
- Killer move
- Countermove
- Quiet moves by history
- Bad noisy moves

### Moves history
- Main history
- Countermove history
- Follow-up move history
- Noisy history

### Time management
- Any time control
- Soft and hard limits
- Nodes TM
