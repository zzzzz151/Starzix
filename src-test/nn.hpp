#ifndef NN_HPP
#define NN_HPP

#include <iostream>
#include "chess.hpp"
#include "../MantaRay/Perspective/PerspectiveNNUE.h"
#include "../MantaRay/Activation/ClippedReLU.h"
using namespace std;
using namespace chess;

// Define the network:
// Format: PerspectiveNetwork<InputType, OutputType, Activation, ...>
//                      <..., InputSize, HiddenSize, OutputSize, ...>
//                      <...,       AccumulatorStackSize,        ...>
//                      <..., Scale, QuantizationFeature, QuantizationOutput>
//using NeuralNetwork = MantaRay::PerspectiveNetwork<int16_t, int32_t, Activation, 768, 64, 1, 512, 400, 255, 64>;
using NeuralNetwork = MantaRay::PerspectiveNetwork<int16_t, int32_t, MantaRay::ClippedReLU<int16_t, 0, 255>, 768, 384, 1, 512, 400, 255, 64>;

// Create the input stream:
MantaRay::MarlinflowStream stream("net1.json");
//MantaRay::BinaryFileStream stream("Aurora-334ab2818f.nnue");

// Create & load the network from the stream:
NeuralNetwork network(stream);

inline int32_t evaluate(Board board)
{
    // Need to call these methods before reactivating features from scratch.
    network.ResetAccumulator();
    network.RefreshAccumulator();

    for (Color color : {Color::WHITE, Color::BLACK})
        for (int i = 0; i <= 5; i++)
        {
            Bitboard bb = board.pieces((PieceType)i, color);
            while (bb > 0)
            {
                int sq = builtin::poplsb(bb);
                network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(i, (int)color, sq);
            }
        }

    return network.Evaluate((int)board.sideToMove());
}

#endif