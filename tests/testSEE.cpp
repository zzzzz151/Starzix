// clang-format off
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "../src/board.hpp"
#include "../src/see.hpp"

int failed = 0, passed = 0;

int main() {
    initZobrist();
    attacks::init();

    std::ifstream inputFile("tests/SEE.txt");

    if (!inputFile.is_open()) {
        std::cout << "Failed to open the file." << std::endl;
        return 1;
    }

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
        else
        {
            std::cout << "FAILED " << fen << " | " << uciMove << " | Expected: " << expected << std::endl;
            failed++;
        }

    }

    inputFile.close();
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return 0;
}