#pragma once

// clang-format off

namespace nnue
{
    const char* NET_FILE = "nn.nnue";
    const uint16_t HIDDEN_LAYER_SIZE = 384;
    const int32_t SCALE = 400, Q = 255 * 64, NORMALIZATION_K = 1;

    struct alignas(64) NN {
        array<int16_t, 768 * HIDDEN_LAYER_SIZE> featureWeights;
        array<int16_t, HIDDEN_LAYER_SIZE> featureBiases;
        array<int8_t , HIDDEN_LAYER_SIZE * 2> outputWeights;
        int16_t outputBias;
    };

    NN nn;

    inline void loadNetFromFile()
    {        
        FILE *netFile;

        #if defined(_WIN32) // windows 32 or 64
        int error = fopen_s(&netFile, NET_FILE, "rb");
        if (error != 0)
        {
            cout << "Error opening net file " << NET_FILE << endl;
            exit(0);
        }

        #elif defined(__unix__) // linux
        netFile = fopen(NET_FILE, "rb");
        if (netFile == NULL)
        {
            cout << "Error opening net file " << NET_FILE << endl;
            exit(0);
        }
        #endif

        // Read binary data (weights and biases) into the struct
        fread(nn.featureWeights.data(), sizeof(int16_t), nn.featureWeights.size(), netFile);
        fread(nn.featureBiases.data(), sizeof(int16_t), nn.featureBiases.size(), netFile);
        fread(nn.outputWeights.data(), sizeof(int8_t), nn.outputWeights.size(), netFile);
        fread(&nn.outputBias, sizeof(int16_t), 1, netFile);
        fclose(netFile); 
    }

    struct Accumulator
    {
        int16_t white[HIDDEN_LAYER_SIZE];
        int16_t black[HIDDEN_LAYER_SIZE];

        inline Accumulator()
        {
            for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
                white[i] = black[i] = nn.featureBiases[i];
        }

        inline void activate(Color color, Square sq, PieceType pieceType)
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

        inline void deactivate(Color color, Square sq, PieceType pieceType)
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

    inline void reset()
    {
        accumulators.clear();
        accumulators.reserve(256);
        accumulators.push_back(Accumulator());
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

    inline int32_t crelu(int32_t x)
    {
        if (x < 0)
            return 0;
        if (x > 255)
            return 255;
        return x;
    }

    inline int32_t evaluate(Color color)
    {
        int16_t *us = currentAccumulator->white,
                *them = currentAccumulator->black;

        if (color == BLACK)
        {
            us = currentAccumulator->black;
            them = currentAccumulator->white;
        }

        int32_t sum = 0;
        for (int i = 0; i < HIDDEN_LAYER_SIZE; i++)
        {
            sum += crelu(us[i]) * nn.outputWeights[i];
            sum += crelu(them[i]) * nn.outputWeights[HIDDEN_LAYER_SIZE + i];
        }

        return (sum / NORMALIZATION_K + nn.outputBias) * SCALE / Q;
    }

}

