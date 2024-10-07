// clang-format off

#include <fstream>
#include "../src/board.hpp"

int main() {
    initUtils();

    std::ifstream inputFile("tests/SEE.txt");
    assert(inputFile.is_open());

    seePawnValue.value  = 100;
    seeMinorValue.value = 300;
    seeRookValue.value  = 500;
    seeQueenValue.value = 900;

    int failed = 0, passed = 0;

    std::string line;
    while (std::getline(inputFile, line))
    {
        std::stringstream iss(line);
        const std::vector<std::string> tokens = splitString(line, '|');

        const std::string fen = tokens[0];
        const std::string uciMove = tokens[1];
        const int gain = stoi(tokens[2]);
        const bool expected = gain >= 0;

        const Board board = Board(fen);
        const bool result = board.SEE(board.uciToMove(uciMove));

        if (result == expected)
            passed++;
        else {
            std::cout << "FAILED " << fen << " | " << uciMove << " | Expected: " << expected << std::endl;
            failed++;
        }

    }

    inputFile.close();
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    return 0;
}
