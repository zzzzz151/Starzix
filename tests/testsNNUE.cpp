// clang-format off
#include "../src/utils.hpp"
#include "../src/board.hpp"
#include "../src/nnue.hpp"
#include "../src/3rdparty/ordered_map.h"

tsl::ordered_map<std::string, int> FENS_EVAL = {
    { START_FEN, 128 },
    { "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",  -64 }, // e2e4
    { "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 183 }, // e2e4 g8f6
    { "rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR w KQkq - 0 1", 128 }, // bongcloud

    // Moves in "r3k2b/6P1/8/1pP5/8/8/4P3/4K2R w Kq b6 0 1"
    { "r3k2b/6P1/1P6/8/8/8/4P3/4K2R b Kq - 0 1", -137 },
    { "r3k2b/6P1/8/1pP5/8/4P3/8/4K2R b Kq - 0 1", 123 },
    { "r3k2b/6P1/8/1pP5/4P3/8/8/4K2R b Kq e3 0 1", 164 },
    { "r3k2b/6P1/2P5/1p6/8/8/4P3/4K2R b Kq - 0 1", 88 },
    { "r3k2Q/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -2400 },
    { "r3k2R/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1608 },
    { "r3k2B/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -832 },
    { "r3k2N/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -464 },
    { "r3k1Qb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1789 },
    { "r3k1Rb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -840 },
    { "r3k1Bb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", 20 },
    { "r3k1Nb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", 273 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/3K3R b q - 1 1", 155 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/5K1R b q - 1 1", 171 },
    { "r3k2b/6P1/8/1pP5/8/8/3KP3/7R b q - 1 1", 73 },
    { "r3k2b/6P1/8/1pP5/8/8/4PK2/7R b q - 1 1", 76 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/5RK1 b q - 1 1", 130 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/4KR2 b q - 1 1", 108 },
    { "r3k2b/6P1/8/1pP5/8/8/4P3/4K1R1 b q - 1 1", 120 },
    { "r3k2b/6P1/8/1pP5/8/8/4P2R/4K3 b q - 1 1", 106 },
    { "r3k2b/6P1/8/1pP5/8/7R/4P3/4K3 b q - 1 1", 88 },
    { "r3k2b/6P1/8/1pP5/7R/8/4P3/4K3 b q - 1 1", 90 },
    { "r3k2b/6P1/8/1pP4R/8/8/4P3/4K3 b q - 1 1", 93 },
    { "r3k2b/6P1/7R/1pP5/8/8/4P3/4K3 b q - 1 1", 7 },
    { "r3k2b/6PR/8/1pP5/8/8/4P3/4K3 b q - 1 1", -80 },
    { "r3k2R/6P1/8/1pP5/8/8/4P3/4K3 b q - 0 1", -1286 },

    // Some positions to test enemy queen buckets
    { "4k3/7n/5q2/8/8/6R1/8/4K3 w - - 0 1", -1047 },
    { "4k3/7n/1q6/8/8/6R1/8/4K3 w - - 0 1", -1036 },
    { "4k3/7n/1q6/8/8/6R1/8/2K5 w - - 0 1", -963 },
    { "4k3/7n/5q2/8/8/6R1/8/2K5 w - - 0 1", -1028 },
    { "q3k3/7n/5q2/8/8/6R1/8/2K5 w - - 0 1", -3662 },
    { "4k3/7n/8/2q5/8/6R1/7q/5K2 w - - 0 1", -3953 },
    { "4k3/7n/8/8/8/6R1/8/5K2 w - - 0 1", 0 },
    { "4k3/7n/8/8/8/6R1/8/1K6 w - - 0 1", 41 },
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
