#include <iostream>
#include <fstream>
#include <string>
#include "src/chess.hpp"
#include "src/see.hpp"

using namespace std;
using namespace chess;

int main()
{
    std::ifstream inputFile("see.epd"); // Replace "example.txt" with the name of your file

    if (!inputFile.is_open())
    {
        std::cerr << "Error opening the file." << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(inputFile, line))
    {
        // Split the line by "; " delimiter
        std::vector<std::string> tokens;
        std::istringstream tokenStream(line);
        std::string token;

        while (std::getline(tokenStream, token, ';'))
        {
            // Remove leading and trailing spaces from each token
            size_t firstNonSpace = token.find_first_not_of(" ");
            size_t lastNonSpace = token.find_last_not_of(" ");
            if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos)
            {
                tokens.push_back(token.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1));
            }
        }

        string fen = tokens[0];
        string move = tokens[1];
        int val = stoi(tokens[2]);
        bool passes = val >= 0;

        Board board = Board(fen);

        bool mySEE = SEE(board, )

        // Process each token
        for (const std::string &token : tokens)
        {
           // std::cout << token << std::endl;
        }
    }

    inputFile.close(); // Close the file when done

    return 0;
}