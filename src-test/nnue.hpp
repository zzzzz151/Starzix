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
// using NeuralNetwork = MantaRay::PerspectiveNetwork<int16_t, int32_t, Activation, 768, 64, 1, 512, 400, 255, 64>;
using NeuralNetwork = MantaRay::PerspectiveNetwork<int16_t, int32_t, MantaRay::ClippedReLU<int16_t, 0, 255>, 768, 128, 1, 512, 400, 255, 64>;

// Create the input stream:
MantaRay::MarlinflowStream stream("net.json");
// MantaRay::BinaryFileStream stream("net.bin");

// Create & load the network from the stream:
NeuralNetwork network(stream);

inline int32_t evaluate(const Board &board)
{
    return network.Evaluate((int)board.sideToMove());
}

inline void restartNNUE()
{
    network.ResetAccumulator();
    network.RefreshAccumulator();

    // Pawns
    for (int sq = 8; sq < 16; sq++)
    {
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(0, 0, sq);
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(0, 1, sq ^ 56);
    }

    // Knights
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(1, 0, 1);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(1, 0, 6);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(1, 1, 1 ^ 56);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(1, 1, 6 ^ 56);

    // Bishops
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(2, 0, 2);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(2, 0, 5);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(2, 1, 2 ^ 56);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(2, 1, 5 ^ 56);

    // Rooks
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(3, 0, 0);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(3, 0, 7);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(3, 1, 0 ^ 56);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(3, 1, 7 ^ 56);

    // Queen
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(4, 0, 3);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(4, 1, 3 ^ 56);

    // King
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(5, 0, 4);
    network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(5, 1, 4 ^ 56);
}

inline void makeMoveAndUpdateNNUE(Board &board, Move &move)
{
    // Push Accumulator (right before updating for makeMove)
    network.PushAccumulator();

    Color color = board.sideToMove();
    int intColor = (int)color;
    Color enemyColor = ~color;
    int intEnemyColor = (int)enemyColor;
    int to = move.to();
    int from = move.from();
    uint16_t moveType = move.typeOf();

    if (moveType == move.CASTLING)
    {
        // Castling is encoded as king captures rook
        int kingFrom = from;
        int rookFrom = to;
        int kingTo, rookTo;
        bool isShortCastle = rookFrom > kingFrom;

        if (isShortCastle)
        {
            kingTo = 6;
            rookTo = 5;
        }
        else
        {
            kingTo = 2;
            rookTo = 3;
        }

        if (color == Color::BLACK)
        {
            kingTo = kingTo ^ 56;
            rookTo = rookTo ^ 56;
        }

        // move king
        network.EfficientlyUpdateAccumulator((int)PieceType::KING, intColor, kingFrom, kingTo);
        // move rook
        network.EfficientlyUpdateAccumulator((int)PieceType::ROOK, intColor, rookFrom, rookTo);

        board.makeMove(move);
        return;
    }

    int pieceType = (int)board.at<PieceType>(move.from());

    if (moveType == move.ENPASSANT)
    {
        int capturedPawnSquare = color == Color::WHITE ? to - 8 : to + 8;
        // remove captured pawn
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>(pieceType, intEnemyColor, capturedPawnSquare);

        // move capturing pawn
        network.EfficientlyUpdateAccumulator(pieceType, intColor, from, to);

        board.makeMove(move);
        return;
    }

    // At this point its not castling and its not en passant

    if (board.isCapture(move))
    {
        PieceType capturedPieceType = board.at<PieceType>(move.to());
        // remove captured piece
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>((int)capturedPieceType, intEnemyColor, to);
    }

    if (moveType == move.PROMOTION)
    {
        // remove promoting pawn
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>(pieceType, intColor, from);

        int promotionPieceType = (int)move.promotionType();
        // add promoted piece
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(promotionPieceType, intColor, to);
    }
    else
    {

        // move piece
        network.EfficientlyUpdateAccumulator(pieceType, intColor, from, to);
    }

    board.makeMove(move);
}

inline void unmakeMoveAndUpdateNNUE(Board &board, Move &move)
{
    board.unmakeMove(move);

    // Pull Accumulator (to undo the updates done after PushAccumulator):
    network.PullAccumulator();
}

#endif