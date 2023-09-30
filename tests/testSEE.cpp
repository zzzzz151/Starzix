#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "../src-test/board/board.hpp"
#include "../src-test/board/attacks.hpp"
#include "../src-test/see.hpp"
using namespace std;

int failed = 0, passed = 0;

int main() {
    Board::initZobrist();
    attacks::initAttacks();
    nnue::loadNetFromFile();

    // Open the file for reading
    std::ifstream inputFile("tests/SEE.epd");

    // Check if the file is open
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return 1; // Return an error code
    }

    std::string line;
    while (std::getline(inputFile, line))
    {
        std::istringstream iss(line);
        std::vector<std::string> tokens;

        // Split the line by " | " delimiter
        std::string part;
        while (std::getline(iss, part, '|')) {
            part.erase(0, part.find_first_not_of(" \t"));
            part.erase(part.find_last_not_of(" \t") + 1);
            part = trim(part);
            tokens.push_back(part);
        }

        string fen = tokens[0];
        string uciMove = tokens[1];
        int gain = stoi(tokens[2]);
        bool expected = gain >= 0;

        Board board = Board(fen);
        Piece pieces[64];
        board.getPieces(pieces);
        Move move = Move::fromUci(uciMove, pieces);

        bool res = SEE(board, move);

        if (res == expected)
            passed++;
        else
        {
            cout << "FAILED " << fen << " | " << uciMove << " | Expected: " << expected << endl;
            failed++;
        }

    }

    // Close the file
    inputFile.close();

    cout << "Passed: " << passed << endl;
    cout << "Failed: " << failed << endl;

    return 0; // Return 0 to indicate success
}