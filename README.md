# Starzix - C++ chess engine

# Elo

[CCRL Blitz](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3626 (#24/753)

[CCRL Rapid](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3521 (#27/597)

[Ipman Bullet](https://ipmanchess.yolasite.com/r9-7945hx.php): 3423 (#38/51)

[Stefan Pohl SPCC](https://www.sp-cc.de/files/uho_full_list.txt): 3544 (#71/88)

# How to compile

Have clang++ installed and run ```make```

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
- Pseudolegal move gen (magic bitboards and lookup tables)
- Copymake make/undo move

### NNUE evaluation 
- (768x2x5 -> 1024)x2 -> 1
- Inputs mirrored along vertical axis based on king square
- 5 enemy queen input buckets
- [Lc0](https://github.com/LeelaChessZero/lc0) data
- Trained with [my trainer](https://github.com/zzzzz151/nn-trainer)

### Search
- Staged move gen
- Fail-soft Negamax
- Principal variation search
- Iterative deepening
- Quiescence search
- Aspiration windows
- Transposition table
- Alpha-beta pruning
- Reverse futility pruning
- Razoring
- Null move pruning
- Probcut
- Late move pruning
- Futility pruning
- SEE pruning
- Internal iterative reduction
- Late move reductions
- Singular extensions
- Correction histories
- Cuckoo (detect upcoming repetition)
- Time management (hard limit, soft limit, nodes TM)
- Multithreading / Lazy SMP

### Move ordering
- TT move
- Good noisy moves by SEE + MVVLVA
- Killer move
- Quiet moves by history
- Bad noisy moves (underpromotions last)

### Moves history
- Main history
- Continuation histories (1 ply, 2 ply, 4 ply)
- Noisy history
- History malus and gravity
