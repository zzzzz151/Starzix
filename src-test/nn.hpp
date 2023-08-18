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

float *crelu(
    int size,          // no need to have any layer structure, we just need the number of elements
    float *output,     // the already allocated storage for the result
    const float *input // the input, which is the output of the previous linear layer
)
{
    for (int i = 0; i < size; ++i)
        output[i] = min(max(input[i], (float)0), (float)1);

    return output + size;
}

const int M = 32;
struct Accumulator
{
    // Two vectors of size M. v[0] for white's, and v[1] for black's perspectives.
    float v[2][M];

    // This will be utilised in later code snippets to make the access less verbose
    float *operator[](Color perspective)
    {
        return v[(int)perspective];
    }
};
Accumulator accumulator = Accumulator{};

struct LinearLayer
{
    int numInputs, numOutputs;
    float **weights;
    float *bias;

    // Constructor
    LinearLayer(int inputs, int outputs) : numInputs(inputs), numOutputs(outputs)
    {
        weights = new float *[numInputs];
        for (int i = 0; i < numInputs; ++i)
            weights[i] = new float[numOutputs];

        bias = new float[numOutputs];
    }

    // Destructor to clean up allocated memory
    ~LinearLayer()
    {
        for (int i = 0; i < numInputs; ++i)
        {
            delete[] weights[i];
        }
        delete[] weights;
        delete[] bias;
    }
};
LinearLayer linearLayer1 = LinearLayer(768, 64);
LinearLayer linearLayer2 = LinearLayer(64, 1);

// The accumulator has to be recomputed from scratch.
void refreshAccumulator(
    const LinearLayer &layer,                // this will always be L_0
    Accumulator &new_acc,                    // storage for the result
    const std::vector<int> &active_features, // the indices of features that are active for this position
    Color perspective                        // the perspective to refresh
)
{
    // First we copy the layer bias, that's our starting point
    for (int i = 0; i < M; ++i)
        new_acc[perspective][i] = layer.bias[i];

    // Then we just accumulate all the columns for the active features. That's what accumulators do!
    for (int a : active_features)
        for (int i = 0; i < M; ++i)
            new_acc[perspective][i] += layer.weights[a][i];
}

// The previous accumulator is reused and just updated with changed features
void updateAccumulator(
    const LinearLayer &layer,                 // this will always be L_0
    Accumulator &new_acc,                     // it's nice to have already provided storage for he new accumulator. Relevant parts will be overwritten
    /*const*/ Accumulator &prev_acc,          // the previous accumulator, the one we're reusing
    const std::vector<int> &removed_features, // the indices of features that were removed
    const std::vector<int> &added_features,   // the indices of features that were added
    Color perspective                         // the perspective to update, remember we have two,  they have separate feature lists, and it even may happen that one is updated while the other needs a full refresh
)
{
    // First we copy the previous values, that's our starting point
    for (int i = 0; i < M; ++i)
        new_acc[perspective][i] = prev_acc[perspective][i];

    // Then we subtract the weights of the removed features
    for (int r : removed_features)
        for (int i = 0; i < M; ++i)
            new_acc[perspective][i] -= layer.weights[r][i]; // Just subtract r-th column

    // Similar for the added features, but add instead of subtracting
    for (int a : added_features)
        for (int i = 0; i < M; ++i)
            new_acc[perspective][i] += layer.weights[a][i];
}

float *linear(
    const LinearLayer &layer, // the layer to use. We have one: linearLayer2
    float *output,            // the already allocated storage for the result
    const float *input        // the input, which is the output of the previous ClippedReLU layer
)
{
    // First copy the biases to the output. We will be adding columns on top of it.
    for (int i = 0; i < layer.numOutputs; ++i)
        output[i] = layer.bias[i];

    // Remember that rainbowy diagram long time ago? This is it.
    // We're adding columns one by one, scaled by the input values.
    for (int i = 0; i < layer.numInputs; ++i)
        for (int j = 0; j < layer.numOutputs; ++j)
            output[j] += input[i] * layer.weights[i][j];

    // Let the caller know where the used buffer ends.
    return output + layer.numOutputs;
}

float evaluate(Board board)
{
    // FeatureSet[N]->M*2->1
    // Linear, N -> M
    // Crelu, M*2
    // Linear M*2 -> 1

    float buffer[64]; // allocate enough space for the results

    // We need to prepare the input first! We will put the accumulator for
    // the side to move first, and the other second.
    float input[2 * M];
    for (int i = 0; i < M; ++i)
    {
        input[i] = accumulator[board.sideToMove()][i];
        input[M + i] = accumulator[~board.sideToMove()][i];
    }

    float *curr_output = buffer;
    float *curr_input = input;
    float *next_output;

    // Evaluate one layer and move both input and output forward.
    // Last output becomes the next input.
    next_output = crelu(2 * linearLayer1.numOutputs, curr_output, curr_input);
    curr_input = curr_output;
    curr_output = next_output;

    next_output = linear(linearLayer2, curr_output, curr_input);

    // We're done. The last layer should have put 1 value out under *curr_output.
    return *curr_output;
}

void onMakeMove()
{
    for (Color perspective : {Color::WHITE, Color::BLACK})
    {
        if (needs_refresh[perspective])
            refreshAccumulator(
                linearLayer1,
                accumulator,
                this->get_active_features(perspective),
                perspective);
        else
            updateAccumulator(linearLayer1,accumulator,
                this->get_previous_position()->accumulator,
                this->get_removed_features(perspective),
                this->get_added_features(perspective),
                perspective);
    }
}

/*
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
*/

#endif