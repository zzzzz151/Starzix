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

inline void makeMoveAndUpdateNNUE(Board &board, Move &move)
{
    /*
    // Push Accumulator (right before updating for makeMove)
    network.PushAccumulator();

    Color color = board.sideToMove();
    int to = move.to();
    int from = move.from();
    uint16_t moveType = move.typeOf();

    if (moveType == move.CASTLING)
    {
        // Castling is encoded as king captures rook
        int kingFrom = from;
        int rookFrom = to;
        int kingTo, rookTo;
        bool isShortCastle = rookFrom - kingFrom > 0;

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
        network.EfficientlyUpdateAccumulator((int)PieceType::KING, (int)color, kingFrom, kingTo);
        // move rook
        network.EfficientlyUpdateAccumulator((int)PieceType::ROOK, (int)color, rookFrom, rookTo);

        board.makeMove(move);
        return;
    }

    int pieceType = (int)board.at<PieceType>(move.from());

    if (moveType == move.ENPASSANT)
    {
        // move capturing pawn
        network.EfficientlyUpdateAccumulator(pieceType, (int)color, from, to);

        int capturedPawnSquare = color == Color::WHITE ? to - 8 : to + 8;
        // remove captured pawn
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>(pieceType, ~(int)color, capturedPawnSquare);

        board.makeMove(move);
        return;
    }

    // At this point its not castling and its not en passant

    bool isCapture = board.isCapture(move);

    if (isCapture)
    {
        int capturedPieceType = (int)board.at<PieceType>(move.to());
        // remove captured piece
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>(capturedPieceType, ~(int)color, to);
    }

    if (moveType == move.PROMOTION)
    {
        // remove promoting pawn
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Deactivate>(pieceType, (int)color, from);

        int promotionPieceType = (int)move.promotionType();
        // add promoted piece
        network.EfficientlyUpdateAccumulator<MantaRay::AccumulatorOperation::Activate>(promotionPieceType, (int)color, to);
    }
    else
    {
        // move piece
        network.EfficientlyUpdateAccumulator(pieceType, (int)color, from, to);
    }
    */

    board.makeMove(move);
}

inline void unmakeMoveAndUpdateNNUE(Board &board, Move &move)
{
    board.unmakeMove(move);

    // Pull Accumulator (to undo the updates done after PushAccumulator):
    //network.PullAccumulator();
}

#endif