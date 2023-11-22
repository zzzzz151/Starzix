#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "../src-test/board.hpp"
#include "../src-test/see.hpp"

int failed = 0, passed = 0;

int main() {
    Board::initZobrist();
    attacks::init();
    //nnue::loadNetFromFile();

    // Open the file for reading
    std::ifstream inputFile("tests/SEE.epd");

    // Check if the file is open
    if (!inputFile.is_open()) {
        std::cout << "Failed to open the file." << std::endl;
        return 1; // Return an error code
    }

    std::string line;
    while (std::getline(inputFile, line))
    {
        std::stringstream iss(line);
        std::vector<std::string> tokens;

        // Split the line by " | " delimiter
        std::string part;
        while (std::getline(iss, part, '|')) {
            part.erase(0, part.find_first_not_of(" \t"));
            part.erase(part.find_last_not_of(" \t") + 1);
            trim(part);
            tokens.push_back(part);
        }

        std::string fen = tokens[0];
        std::string uciMove = tokens[1];
        int gain = stoi(tokens[2]);
        bool expected = gain >= 0;

        Board board = Board(fen);
        Move move = Move::fromUci(uciMove, board.getPieces());
        bool res = see::SEE(board, move);

        if (res == expected)
            passed++;
        else
        {
            std::cout << "FAILED " << fen << " | " << uciMove << " | Expected: " << expected << std::endl;
            failed++;
        }

    }

    // Close the file
    inputFile.close();

    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return 0; // Return 0 to indicate success
}