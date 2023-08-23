#ifndef NN_HPP
#define NN_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <immintrin.h> // Include the appropriate SIMD header
#include "weights.hpp"
#include "chess.hpp"
using namespace std;
using namespace chess;

float sigmoid(float x)
{
    return 1.0 / (1.0 + exp(-x));
}

float relu(float x)
{
    return (x > 0) ? x : 0;
}

float linear(float x)
{
    return x;
}

int predict(int input[769])
{
    float dense32[32];
    for (int i = 0; i < 32; ++i)
    {
        float sum = 0.0;
        for (int j = 0; j < 769; ++j)
            sum += input[j] * weightsDense32[j][i];
        sum += biasesDense32[i];
        dense32[i] = relu(sum);
    }

    float output = 0.0;
    for (int i = 0; i < 32; ++i)
        output += dense32[i] * weightsOutput[i];
    output += biasOutput;

    if (output < 0)
        return (int)(output - 0.5);
    if (output > 0)
        return (int)(output + 0.5);
    return 0;
}

int evaluate(Board board)
{
    int input[769];
    input[768] = (int)board.sideToMove(); // last bit is color to move

    int currentInputIndex = 0;
    for (Color color : {Color::WHITE, Color::BLACK})
    {
        // Iterate piece types
        for (int i = 0; i <= 5; i++)
        {
            Bitboard bb = board.pieces((PieceType)i, color);
            // bb = __builtin_bswap64(bb); // convert to how humans visualize a board
            bitset<64> bits(bb);
            for (int i = 0; i < 64; ++i)
                input[currentInputIndex++] = bits[i];
        }
    }

    return predict(input);
}

#endif