// clang-format off

#include "../src/position.hpp"
#include <cassert>
#include <fstream>

int main() {
    std::cout << colored("Running SEE tests...", ColorCode::Yellow) << std::endl;

    pawnValue.value  = 100;
    minorValue.value = 300;
    rookValue.value  = 500;
    queenValue.value = 900;

    std::ifstream inputFile("tests/SEE.txt");
    assert(inputFile.is_open());

    std::string line = "";
    while (std::getline(inputFile, line))
    {
        std::stringstream iss(line);
        const std::vector<std::string> tokens = splitString(line, '|');

        const std::string fen = tokens[0];
        const std::string uciMove = tokens[1];
        const i32 gain = stoi(tokens[2]);

        const Position pos = Position(fen);
        const Move move = pos.uciToMove(uciMove);

        assert(pos.SEE(move, gain - 1));
        assert(pos.SEE(move, gain));
        assert(!pos.SEE(move, gain + 1));
    }

    inputFile.close();
    std::cout << colored("SEE tests passed", ColorCode::Green) << std::endl;
}