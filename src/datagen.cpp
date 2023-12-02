// clang-format off

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include "board.hpp"

Board board;

#include "search.hpp"
#include "uci.hpp"

const std::string OUTPUT_FOLDER = "data";

const u8 MIN_RANDOM_PLIES = 11,
         MAX_RANDOM_PLIES = 12;

const u32 SOFT_NODES = 5000,
          HARD_NODES = 8'000'000;

const i16 MAX_OPENING_SCORE = 300,
          ADJUDICATION_SCORE = 1000;

inline bool isCheckmateOrStalemate(Board &board);

int main(int argc, char* argv[])
{
    // Create output folder if doesnt exist
    if (!std::filesystem::exists(OUTPUT_FOLDER))
        std::filesystem::create_directory(OUTPUT_FOLDER);

    // Random file name
    std::string filePath = OUTPUT_FOLDER + "/" + getRandomString(12) + ".txt";

    // Open the file for writing
    std::ofstream outputFile(filePath);

    if (!outputFile.is_open()) {
        std::cerr << "Error creating the file " << filePath << std::endl;
        exit(1);
    }

    std::cout << "Generating data to " << filePath << std::endl;

    attacks::init();
    nnue::loadNetFromFile();
    search::init();
    uci::outputSearchInfo = false;

    // rng
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distribution(MIN_RANDOM_PLIES, MAX_RANDOM_PLIES);

    u64 totalPositions = 0;
    std::array<std::string, 256> lines; // <FEN> | <scoreWhitePerspective> | <1.0 if white win OR 0.5 if draw OR 0.0 if white loss>

    TimeManager timeManager = TimeManager();
    timeManager.softNodes = SOFT_NODES;
    timeManager.hardNodes = HARD_NODES;

    const std::chrono::time_point start = std::chrono::steady_clock::now();

    while (true)
    {
        runNewGame:

        board = Board(START_FEN);
        int numRandomPlies = distribution(gen);

        for (int i = 0; i < numRandomPlies; i++)
        {
            MovesList moves = board.pseudolegalMoves();
            moves.shuffle();

            int j = 0;
            for (j = 0; j < moves.size(); j++)
                if (board.makeMove(moves[j])) break;

            // if no legal move, its stalemate or checkmate, so generate another random opening
            if (j == moves.size()) goto runNewGame;
        }

        uci::ucinewgame();

        // If the random opening is too bad, generate another random opening
        auto [bestMove, score] = search::search(10);
        if (abs(score) >= MAX_OPENING_SCORE) goto runNewGame;

        uci::ucinewgame();

        int numLines = 0;
        std::string wdl = "";

        // Play the game out
        while (true)
        {
            timeManager.restart();
            auto [bestMove, score] = search::search(timeManager);

            if (abs(score) >= ADJUDICATION_SCORE)
            {
                wdl = board.sideToMove() == Color::WHITE 
                      ? (score >= ADJUDICATION_SCORE ? "1.0" : "0.0")
                      : (score >= ADJUDICATION_SCORE ? "0.0" : "1.0");
                break;
            }

            if (!board.inCheck() && bestMove.promotion() == PieceType::NONE && !board.isCapture(bestMove))
            {
                // Transform into white perspective score
                if (board.sideToMove() == Color::BLACK)
                    score = -score;
                    
                lines[numLines++] = board.fen() + " | " + std::to_string(score);
                if (numLines == lines.size()) goto runNewGame;
            }

            board.makeMove(bestMove);

            if (board.isDraw())
            {
                wdl = "0.5";
                break;
            }

            if (isCheckmateOrStalemate(board))
            {
                wdl = board.inCheck() 
                      ? (board.sideToMove() == Color::WHITE ? "0.0" : "1.0")
                      : "0.5";
                break;
            }
        }
        
        // Write the game we just played to the output file
        for(int i = 0; i < numLines; i++)
            outputFile << lines[i] + " | " + wdl << std::endl;

        totalPositions += numLines;
        u64 positionsPerSec = totalPositions * 1000 / millisecondsElapsed(start);
        std::cout << "(" << filePath   
                  << ") Total positions: " << totalPositions
                  << ", positions/sec: " << positionsPerSec 
                  << std::endl;

        goto runNewGame;
    }

    outputFile.close();
    std::cout << std::endl << "Closed file " << filePath << std::endl;

    return 0;
}

inline bool isCheckmateOrStalemate(Board &board)
{
    MovesList moves = board.pseudolegalMoves();

    for (int i = 0; i < moves.size(); i++)
    {
        if (board.makeMove(moves[i]))
        {
            board.undoMove();
            return false;
        }
    }

    return true;
}

