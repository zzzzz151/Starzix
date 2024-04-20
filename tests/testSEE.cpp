// clang-format off
#include <fstream>
#include "../src-test/board.hpp"
#include "../src-test/see.hpp"

int main() {
    attacks::init();
    initUtils();
    initZobrist();

    std::ifstream inputFile("tests/SEE.txt");
    assert(inputFile.is_open());

    seePawnValue.value = 100;
    seeMinorValue.value = 300;
    seeRookValue.value = 500;
    seeQueenValue.value = 900;

    int failed = 0, passed = 0;

    std::string line;
    while (std::getline(inputFile, line))
    {
        std::stringstream iss(line);
        std::vector<std::string> tokens = splitString(line, '|');

        std::string fen = tokens[0];
        std::string uciMove = tokens[1];
        int gain = stoi(tokens[2]);
        bool expected = gain >= 0;

        Board board = Board(fen);
        bool result = SEE(board, board.uciToMove(uciMove));

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