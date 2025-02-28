// clang-format off

#include "../src/position.hpp"
#include "../src/nnue.hpp"
#include "positions.hpp"
#include "../3rd-party/ordered_map.h"
#include <cassert>

tsl::ordered_map<std::string, i32> FENS_EVAL = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 46 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 w - - 0 1", 558 },
    { "r3k1r1/8/ppp5/3p4/8/1B6/1B4N1/4K1R1 b - - 0 1", -393 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 w - - 0 1", 83 },
    { "r5r1/3k4/ppp5/3p4/1K6/1B6/1B4N1/6R1 b - - 0 1", 29 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 w - - 0 1", 325 },
    { "r5r1/3k4/ppp5/3p4/8/1B6/1B3KN1/6R1 b - - 0 1", -200 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 w - - 0 1", 447 },
    { "r5r1/8/ppp5/3p4/8/1B6/1B4Nk/1K4R1 b - - 0 1", -298 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 w - - 0 1", 751 },
    { "b2qk3/6pp/8/8/8/5Q1N/R7/4K3 b - - 0 1", -558 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 w - - 0 1", 1024 },
    { "b3k3/6pp/8/8/8/1Q5N/Rq6/4K3 b - - 0 1", -799 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 w - - 0 1", 2491 },
    { "b3k3/6pp/5Q2/8/8/7N/R7/4K3 b - - 0 1", -2201 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 w - - 0 1", -1339 },
    { "b3k3/6pp/8/8/3q4/7N/R7/4K3 b - - 0 1", 1460 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 w - - 0 1", 2958 },
    { "b3k3/5qpp/8/8/8/2Q4N/R1Q5/4K3 b - - 0 1", -2879 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 w - - 0 1", -4948 },
    { "b3k1q1/6pp/8/8/8/1q5N/R7/4K3 b - - 0 1", 5123 },
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
