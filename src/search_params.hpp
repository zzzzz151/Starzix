// clang-format off

#pragma once

#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max, step;

    inline TunableParam(const std::string name, T value, T min, T max, T step) 
    : name(name), value(value), min(min), max(max), step(step) { }
};

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>("aspMinDepth", 8, 6, 10, 1);
TunableParam<i32> aspInitialDelta = TunableParam<i32>("aspInitialDelta", 11, 5, 25, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.51, 1.2, 2.0, 0.1);

// RFP (Reverse futility pruning) / Static NMP
TunableParam<i32> rfpMaxDepth = TunableParam<i32>("rfpMaxDepth", 8, 6, 10, 1);
TunableParam<i32> rfpDepthMultiplier = TunableParam<i32>("rfpDepthMultiplier", 68, 40, 130, 10);

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>("razoringMaxDepth", 5, 2, 6, 2);
TunableParam<i32> razoringDepthMultiplier = TunableParam<i32>("razoringDepthMultiplier", 290, 250, 450, 100);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>("nmpMinDepth", 3, 2, 4, 1);
TunableParam<float> nmpBaseReduction = TunableParam<float>("nmpBaseReduction", 2.48, 2.0, 4.0, 0.5);
TunableParam<float> nmpReductionDivisor = TunableParam<float>("nmpReductionDivisor", 2.43, 2.0, 4.0, 0.5);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>("nmpEvalBetaDivisor", 212, 100, 300, 50);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>("nmpEvalBetaMax", 4, 1, 5, 2);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>("iirMinDepth", 5, 4, 6, 1);

// LMP (Late move pruning)
TunableParam<i32> lmpMinMoves = TunableParam<i32>("lmpMinMoves", 2, 2, 4, 1);
TunableParam<float> lmpDepthMultiplier = TunableParam<float>("lmpDepthMultiplier", 0.95, 0.5, 1.5, 0.1);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>("fpMaxDepth", 7, 6, 10, 1);
TunableParam<i32> fpBase = TunableParam<i32>("fpBase", 160, 40, 260, 22);
TunableParam<i32> fpMultiplier = TunableParam<i32>("fpMultiplier", 185, 40, 260, 22);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>("seePruningMaxDepth", 8, 7, 11, 1);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>("seeNoisyThreshold", -13, -51, -1, 10);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>("seeQuietThreshold", -53, -101, -1, 20);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>("singularMinDepth", 7, 6, 10, 1);
TunableParam<i32> singularDepthMargin = TunableParam<i32>("singularDepthMargin", 2, 1, 5, 1);
TunableParam<float> singularBetaMultiplier = TunableParam<float>("singularBetaMultiplier", 2.04, 1.0, 3.0, 0.5);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>("doubleExtensionMargin", 20, 2, 42, 10);
TunableParam<u8> doubleExtensionsMax = TunableParam<u8>("doubleExtensionsMax", 6, 4, 10, 2);

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.69, 0.4, 1.2, 0.1);
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.38, 0.2, 0.8, 0.1);
TunableParam<i32> lmrMinMoves = TunableParam<i32>("lmrMinMoves", 3, 2, 4, 1);
TunableParam<i32> lmrQuietHistoryDiv = TunableParam<i32>("lmrQuietHistoryDiv", 6677, 2048, 32768, 2048);
TunableParam<i32> lmrNoisyHistoryDiv = TunableParam<i32>("lmrNoisyHistoryDiv", 1806, 16, 32768, 2047);

// History
TunableParam<i32> historyMax = TunableParam<i32>("historyMax", 10389, 8192, 32768, 2048);
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>("historyBonusMultiplier", 274, 50, 600, 50);
TunableParam<i32> historyBonusOffset = TunableParam<i32>("historyBonusOffset", 57, 0, 1000, 100);
TunableParam<i32> historyBonusMax = TunableParam<i32>("historyBonusMax", 1351, 500, 2500, 100);
TunableParam<i32> historyMalusMultiplier = TunableParam<i32>("historyMalusMultiplier", 521, 50, 600, 50);
TunableParam<i32> historyMalusOffset = TunableParam<i32>("historyMalusOffset", 90, 0, 1000, 100);
TunableParam<i32> historyMalusMax = TunableParam<i32>("historyMalusMax", 1029, 500, 2500, 100);
TunableParam<float> historyBonusMultiplierMain = TunableParam<float>("historyBonusMultiplierMain", 1.07, 0.25, 4.0, 0.25);
TunableParam<float> historyBonusMultiplierNoisy = TunableParam<float>("historyBonusMultiplierNoisy", 0.69, 0.25, 4.0, 0.25);
TunableParam<float> historyBonusMultiplier1Ply = TunableParam<float>("historyBonusMultiplier1Ply", 1.25, 0.25, 4.0, 0.25);
TunableParam<float> historyBonusMultiplier2Ply = TunableParam<float>("historyBonusMultiplier2Ply", 1.05, 0.25, 4.0, 0.25);
TunableParam<float> historyBonusMultiplier4Ply = TunableParam<float>("historyBonusMultiplier4Ply", 0.95, 0.25, 4.0, 0.25);

// Time Management
TunableParam<u64> defaultMovesToGo = TunableParam<u64>("defaultMovesToGo", 21, 20, 26, 3);
TunableParam<double> hardTimePercentage = TunableParam<double>("hardTimePercentage", 0.57, 0.3, 0.6, 0.15);
TunableParam<double> softTimePercentage = TunableParam<double>("softTimePercentage", 0.69, 0.5, 0.7, 0.1);

// Nodes time management (scale soft time based on best move nodes fraction)
TunableParam<i32> nodesTmMinDepth = TunableParam<i32>("nodesTmMinDepth", 10, 7, 11, 1);
TunableParam<double> nodesTmBase = TunableParam<double>("nodesTmBase", 1.49, 1.4, 1.6, 0.1);
TunableParam<double> nodesTmMultiplier = TunableParam<double>("nodesTmMultiplier", 1.5, 1.3, 1.5, 0.1);

// SEE piece values
TunableParam<i32> seePawnValue = TunableParam<i32>("seePawnValue", 185, 0, 200, 40);
TunableParam<i32> seeMinorValue = TunableParam<i32>("seeMinorValue", 296, 150, 450, 50);
TunableParam<i32> seeRookValue = TunableParam<i32>("seeRookValue", 378, 300, 700, 50);
TunableParam<i32> seeQueenValue = TunableParam<i32>("seeQueenValue", 722, 600, 1200, 100);

using TunableParamVariant = std::variant<
    TunableParam<u8>*,
    TunableParam<u64>*,
    TunableParam<i32>*, 
    TunableParam<double>*, 
    TunableParam<float>*
>;

std::vector<TunableParamVariant> tunableParams 
{
    &aspMinDepth, &aspInitialDelta, &aspDeltaMultiplier,
    &rfpMaxDepth, &rfpDepthMultiplier,
    &razoringMaxDepth, &razoringDepthMultiplier,
    &nmpMinDepth, &nmpBaseReduction, &nmpReductionDivisor, &nmpEvalBetaDivisor, &nmpEvalBetaMax,
    &iirMinDepth, 
    &lmpMinMoves, &lmpDepthMultiplier,
    &fpMaxDepth, &fpBase, &fpMultiplier,
    &seePruningMaxDepth, &seeNoisyThreshold, &seeQuietThreshold,
    &singularMinDepth, &singularDepthMargin, &singularBetaMultiplier,
    &doubleExtensionMargin, &doubleExtensionsMax,
    &lmrBase, &lmrMultiplier, 
    &lmrMinMoves,
    &lmrQuietHistoryDiv, &lmrNoisyHistoryDiv,
    &historyMax,
    &historyBonusMultiplier, &historyBonusOffset, &historyBonusMax,
    &historyMalusMultiplier, &historyMalusOffset, &historyMalusMax,
    &historyBonusMultiplierMain, &historyBonusMultiplierNoisy,
    &historyBonusMultiplier1Ply, &historyBonusMultiplier2Ply, &historyBonusMultiplier4Ply,
    &defaultMovesToGo, &hardTimePercentage, &softTimePercentage,
    &nodesTmMinDepth, &nodesTmBase, &nodesTmMultiplier,
    &seePawnValue, &seeMinorValue, &seeRookValue, &seeQueenValue
};

inline void printParamsAsJson()
{
    /*
    {
        "Param": {
            "value": 50,
            "min_value": 0,
            "max_value": 100,
            "step": 10
        }
    }
    */

    std::string indent = "    "; // 4 spaces
    std::cout << "{" << std::endl;

    for (auto &myTunableParam : tunableParams) 
    {
        std::visit([&indent](auto &tunableParam) 
        {
            std::cout << indent << "\"" << tunableParam->name << "\"" << ": {" << std::endl;

            std::cout << indent << indent 
                      << "\"value\": " 
                      << (std::is_same<decltype(tunableParam->value), double>::value
                         || std::is_same<decltype(tunableParam->value), float>::value
                         ? tunableParam->value * 100.0
                         : (i64)tunableParam->value)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"min_value\": " 
                      << (std::is_same<decltype(tunableParam->min), double>::value
                         || std::is_same<decltype(tunableParam->min), float>::value
                         ? tunableParam->min * 100.0
                         : (i64)tunableParam->min)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"max_value\": " 
                      << (std::is_same<decltype(tunableParam->max), double>::value
                         || std::is_same<decltype(tunableParam->max), float>::value
                         ? tunableParam->max * 100.0
                         : (i64)tunableParam->max)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"step\": " 
                      << (std::is_same<decltype(tunableParam->step), double>::value
                         || std::is_same<decltype(tunableParam->step), float>::value
                         ? tunableParam->step * 100.0
                         : (i64)tunableParam->step)
                      << std::endl;

            std::cout << indent << "}," << std::endl;

        }, myTunableParam);
    }

    std::cout << "}" << std::endl;
}


