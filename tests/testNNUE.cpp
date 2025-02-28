// clang-format off

#include "../src/position.hpp"
#include "../src/nnue.hpp"
#include "positions.hpp"
#include "../3rd-party/ordered_map.h"
#include <cassert>

tsl::ordered_map<std::string, i32> FENS_EVAL = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 51 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 w - - 0 1", 578 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 b - - 0 1", -405 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 w - - 0 1", 94 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 b - - 0 1", 37 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 w - - 0 1", 313 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 b - - 0 1", -166 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 w - - 0 1", 444 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 b - - 0 1", -293 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 w - - 0 1", 785 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 b - - 0 1", -593 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 w - - 0 1", 992 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 b - - 0 1", -775 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 w - - 0 1", 2441 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 b - - 0 1", -2130 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 w - - 0 1", -1355 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 b - - 0 1", 1483 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 w - - 0 1", 2919 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 b - - 0 1", -2848 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 w - - 0 1", -5049 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 b - - 0 1", 5232 },
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
