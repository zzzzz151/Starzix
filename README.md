# Starzix

Starzix is a strong C++ chess engine that communicates using [UCI](https://www.chessprogramming.org/UCI).

The search is a standard fail-soft negamax principal variation search with various enhancements such as alpha-beta pruning, quiescence search and transposition table.

For evaluation, it uses a `(768x2x5 -> 1024)x2 -> 1` horizontally mirrored NNUE trained on [Lc0](https://github.com/LeelaChessZero/lc0) data with my trainer [Starway](https://github.com/zzzzz151/Starway).

# Elo (v6.0)

[CCRL Blitz 8 Threads](https://www.computerchess.org.uk/ccrl/404/): 3761 (#10/783)

[CCRL Blitz 1 Thread](https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3699 (#11/779)

[CCRL Rapid 4 Threads](https://www.computerchess.org.uk/ccrl/4040/): 3603 (#10/627)

[CCRL Rapid 1 Thread](https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no): 3566 (#13/627)

[Stefan Pohl SPCC](https://www.sp-cc.de/): 3638 (#15/16)

[Ipman Bullet](https://ipmanchess.yolasite.com/r9-7945hx.php): 3531 (#21/51)

# How to compile

Have clang++ and run `make`

# UCI options

- Hash (integer, default 32, 1 to 131072) - transposition table size in MiB

- Threads (integer, default 1, 1 to 512) - search threads

# Extra commands

- display

- perft \<depth\>

- perftsplit \<depth\>

- bench \<depth\>

- eval
