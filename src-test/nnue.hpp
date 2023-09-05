#ifndef NNUE_HPP
#define NNUE_HPP

#include <iostream>
#include "../MantaRay/Perspective/PerspectiveNNUE.h"
#include "../MantaRay/Activation/ClippedReLU.h"
using namespace std;

// Define the network:
// Format: PerspectiveNetwork<InputType, OutputType, Activation, ...>
//                      <..., InputSize, HiddenSize, OutputSize, ...>
//                      <...,       AccumulatorStackSize,        ...>
//                      <..., Scale, QuantizationFeature, QuantizationOutput>
using NeuralNetwork = MantaRay::PerspectiveNetwork<int16_t, int32_t, MantaRay::ClippedReLU<int16_t, 0, 255>, 768, 128, 1, 512, 400, 255, 64>;

// Create the input stream:
MantaRay::MarlinflowStream stream("net.json");
//MantaRay::BinaryFileStream stream("net.bin");

// Create & load the network from the stream:
NeuralNetwork network(stream);

#endif