#ifndef NNUE_HPP
#define NNUE_HPP

#include <iostream>
#include <fstream>
#include <array>
#include <cstdint>
#include <vector>
#include <cassert>
#include "board/types.hpp"
#include "board/builtin.hpp"
using namespace std;

// clang-format off
namespace nnue
{
    const string NET_FILE = "net_128_500M_0wdl.nnue";
    const uint16_t HIDDEN_LAYER_SIZE = 128;
    const int32_t Q = 255 * 64;

    struct alignas(64) NN {
        array<int16_t, 768 * HIDDEN_LAYER_SIZE> featureWeights;
        array<int16_t, HIDDEN_LAYER_SIZE> featureBiases;
        array<int8_t , HIDDEN_LAYER_SIZE* 2> outputWeights;
        int16_t outputBias;
    };

    NN nn;

    inline void loadNetFromFile()
    {
        ifstream file(NET_FILE, ios::binary);

        if (!file.is_open()) {
            cout << "Error opening net file " << NET_FILE << endl;
            exit(0);
        }

        file.read(reinterpret_cast<char*>(&nn), sizeof(NN));
        file.close();
    }

    struct Accumulator
    {
        int16_t white[HIDDEN_LAYER_SIZE];
        int16_t black[HIDDEN_LAYER_SIZE];

        inline Accumulator(uint64_t piecesBitboards[6][2] = nullptr)
        {
            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
                white[i] = black[i] = nn.featureBiases[i];

            if (piecesBitboards == nullptr)
                return;

            for (int pt = 0; pt <= 5; pt++)
            {
                for (int color = 0; color <= 1; color++)
                {
                    uint64_t bb = piecesBitboards[pt][color];
                    while (bb > 0)
                    {
                        Square sq = poplsb(bb);
                        activate(color, (PieceType)pt, sq);
                    }
                }
            }
        }

        inline void activate(Color color, PieceType pieceType, Square sq)
        {
            int whiteIdx = color * 384 + (int)pieceType * 64 + sq;
            int blackIdx = !color * 384 + (int)pieceType * 64 + (sq ^ 56);
            int whiteOffset = whiteIdx * HIDDEN_LAYER_SIZE;
            int blackOffset = blackIdx * HIDDEN_LAYER_SIZE;

            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            {
                white[i] += nn.featureWeights[whiteOffset + i];
                black[i] += nn.featureWeights[blackOffset + i];
            }
        }   

        inline void deactivate(Color color, PieceType pieceType, Square sq)
        {
            int whiteIdx = color * 384 + (int)pieceType * 64 + sq;
            int blackIdx = !color * 384 + (int)pieceType * 64 + (sq ^ 56);
            int whiteOffset = whiteIdx * HIDDEN_LAYER_SIZE;
            int blackOffset = blackIdx * HIDDEN_LAYER_SIZE;

            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
            {
                white[i] -= nn.featureWeights[whiteOffset + i];
                black[i] -= nn.featureWeights[blackOffset + i];
            }
        }
    };

    vector<Accumulator> accumulators;
    Accumulator *currentAccumulator;

    inline void reset(uint64_t piecesBitboards[6][2] = nullptr)
    {
        accumulators.clear();
        accumulators.reserve(256);
        accumulators.push_back(Accumulator(piecesBitboards));
        currentAccumulator = &accumulators.back();
    }

    inline void push()
    {
        assert(currentAccumulator == &accumulators.back());
        accumulators.push_back(*currentAccumulator);
        currentAccumulator = &accumulators.back();
    }

    inline void pull()
    {
        accumulators.pop_back();
        currentAccumulator = &accumulators.back();
    }

    inline int32_t crelu(int16_t x)
    {
        if (x < 0)
            return 0;
        if (x > 255)
            return 255;
        return x;
    }

    inline int32_t evaluate(Color color)
    {
        int16_t *us, *them;
        if (color == WHITE)
        {
            us = currentAccumulator->white;
            them = currentAccumulator->black;
        }
        else
        {
            us = currentAccumulator->black;
            them = currentAccumulator->white;
        }

        int32_t sum = nn.outputBias;
        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            sum += crelu(us[i]) * nn.outputWeights[i];
            sum += crelu(them[i]) * nn.outputWeights[HIDDEN_LAYER_SIZE + i];
        }

        return sum * 400.0 / Q;
    }

}

#endif