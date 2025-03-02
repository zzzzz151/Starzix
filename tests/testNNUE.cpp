// clang-format off

#include "../src/position.hpp"
#include "../src/nnue.hpp"
#include "positions.hpp"
#include "../3rd-party/ordered_map.h"
#include <cassert>

tsl::ordered_map<std::string, i32> FENS_EVAL = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 26 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 w - - 0 1", 762 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 b - - 0 1", -646 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 w - - 0 1", 81 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 b - - 0 1", -36 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 w - - 0 1", 311 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 b - - 0 1", -229 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 w - - 0 1", 988 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 b - - 0 1", -920 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 w - - 0 1", 921 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 b - - 0 1", -644 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 w - - 0 1", 1343 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 b - - 0 1", -1074 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 w - - 0 1", 1669 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 b - - 0 1", -1741 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 w - - 0 1", -1351 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 b - - 0 1", 1541 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 w - - 0 1", 3370 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 b - - 0 1", -2991 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 w - - 0 1", -4827 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 b - - 0 1", 5054 },
};

int main()
{
    std::cout << colored("Running NNUE tests...", ColorCode::Yellow) << std::endl;

    for (const auto& [fen, expectedEval] : FENS_EVAL)
    {
        const Position pos = Position(fen);
        const nnue::BothAccumulators bothAccs = nnue::BothAccumulators(pos);
        const auto eval = nnue::evaluate(bothAccs, pos.sideToMove());
        assert(std::abs(eval - expectedEval) <= 1);
    }

    std::cout << colored("NNUE tests passed", ColorCode::Green) << std::endl;
}
