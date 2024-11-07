// clang-format off
#include "../src/utils.hpp"
#include "../src/board.hpp"
#include "../src/nnue.hpp"
#include "../src/3rdparty/ordered_map.h"

tsl::ordered_map<std::string, int> FENS_EVAL = {
    { START_FEN, 56 },
    { "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",  -23 }, // e2e4
    { "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 104 }, // e2e4 g8f6
    { "rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR w KQkq - 0 1", 56 }, // bongcloud

    // Moves in "r3k2b/6P1/8/1pP5/8/8/4P3/4K2R w Kq b6 0 1"
    { "r3k2b/6P1/1P6/8/8/8/4P3/4K2R b Kq - 0 1", -319 },
    { "r3k2b/6P1/8/1pP5/8/4P3/8/4K2R b Kq - 0 1", 102 },
    { "r3k2b/6P1/8/1pP5/4P3/8/8/4K2R b Kq e3 0 1", 89 },
    { "r3k2b/6P1/2P5/1p6/8/8/4P3/4K2R b Kq - 0 1", 75 },
    { "r3k2Q/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -2365 },
    { "r3k2R/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1930 },
    { "r3k2B/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -873 },
    { "r3k2N/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -652 },
    { "r3k1Qb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1534 },
    { "r3k1Rb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -896 },
    { "r3k1Bb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", 3 },
    { "r3k1Nb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", 168 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/3K3R b q - 1 1", 121 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/5K1R b q - 1 1", 195 },
    { "r3k2b/6P1/8/1pP5/8/8/3KP3/7R b q - 1 1", 51 },
    { "r3k2b/6P1/8/1pP5/8/8/4PK2/7R b q - 1 1", 101 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/5RK1 b q - 1 1", 149 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/4KR2 b q - 1 1", 79 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/4K1R1 b q - 1 1", 113 },
    { "r3k2b/6P1/8/1pP5/8/8/4P2R/4K3 b q - 1 1", 115 },
    { "r3k2b/6P1/8/1pP5/8/7R/4P3/4K3 b q - 1 1", 83 },
    { "r3k2b/6P1/8/1pP5/7R/8/4P3/4K3 b q - 1 1", 62 },
    { "r3k2b/6P1/8/1pP4R/8/8/4P3/4K3 b q - 1 1", -12 },
    { "r3k2b/6P1/7R/1pP5/8/8/4P3/4K3 b q - 1 1", -14 },
    { "r3k2b/6PR/8/1pP5/8/8/4P3/4K3 b q - 1 1", -55 },
    { "r3k2R/6P1/8/1pP5/8/8/4P3/4K3 b q - 0 1", -1370 },

    // Some positions to test enemy queen buckets
    { "4k3/7n/5q2/8/8/6R1/8/4K3 w - - 0 1", -1339 },
    { "4k3/7n/1q6/8/8/6R1/8/4K3 w - - 0 1", -1380 },
    { "4k3/7n/1q6/8/8/6R1/8/2K5 w - - 0 1", -1257 },
    { "4k3/7n/5q2/8/8/6R1/8/2K5 w - - 0 1", -1324 },
    { "q3k3/7n/5q2/8/8/6R1/8/2K5 w - - 0 1", -4697 },
    { "4k3/7n/8/2q5/8/6R1/7q/5K2 w - - 0 1", -4965 },
    { "4k3/7n/8/8/8/6R1/8/5K2 w - - 0 1", 42 },
    { "4k3/7n/8/8/8/6R1/8/1K6 w - - 0 1", 16 },
};

int main()
{
    for (const auto& [fen, expectedEval] : FENS_EVAL) 
    {
        const Board board = Board(fen);
        const BothAccumulators bothAccs = BothAccumulators(board);
        const auto eval = nnue::evaluate(&bothAccs, board.sideToMove());

        if (eval != expectedEval) 
            std::cout << "Expected eval " << expectedEval
                      << " but got " << eval 
                      << " in '" << fen << "'" 
                      << std::endl;
    }

    std::cout << "Finished" << std::endl;
    return 0;
}
