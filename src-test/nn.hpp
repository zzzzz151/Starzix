#ifndef NN_HPP
#define NN_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include "weights.hpp"
#include "chess.hpp"
using namespace std;
using namespace chess;

int PIECE_VALUES[7] = {100, 302, 320, 500, 900, 15000, 0};

double sigmoid(double x)
{
    return 1.0 / (1.0 + exp(-x));
}

double relu(double x)
{
    return (x > 0) ? x : 0;
}

double linear(double x)
{
    return x;
}

int predict(double input[769])
{
    double dense[128];
    for (int i = 0; i < 32; ++i)
    {
        double sum = 0.0;
        for (int j = 0; j < 769; ++j)
            sum += input[j] * weightsInputToDense[j][i];
        sum += biasesDense[i];
        dense[i] = relu(sum);
    }

    double output = 0.0;
    for (int i = 0; i < 32; ++i)
        output += dense[i] * weightsDenseToOutput[i];
    output += biasOutput;

    if (output < 0)
        return (int)(output - 0.5);
    if (output > 0)
        return (int)(output + 0.5);
    return 0;
}

int evaluate(Board board)
{
    double input[769];
    input[768] = board.sideToMove() == Color::WHITE ? 0 : 1; // last bit is color to move

    // 12 8x8 bitboards, first white then black, pawns->knights->bishops->rooks->queens->king

    int currentInputIndex = 0;
    for (Color color : {Color::WHITE, Color::BLACK})
    {
        // Iterate piece types
        for (int i = 0; i <= 5; i++)
        {
            Bitboard bb = board.pieces((PieceType)i, color);
            bb = __builtin_bswap64(bb); // convert to how humans visualize a board
            bitset<64> bits(bb);
            for (int i = 0; i < 64; ++i)
                input[currentInputIndex++] = bits[i];
        }
    }

    return board.sideToMove() == Color::WHITE ? predict(input) : -predict(input);
}

int testNN()
{
    double input[769];
    for (int i = 0; i < 769; i++)
        input[i] = i % 2;

    cout << "Prediction: " << predict(input) << endl;

    return 0;
}

#endif