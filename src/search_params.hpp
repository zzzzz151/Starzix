// clang-format off

#pragma once

#include <variant>

template <typename T> struct TunableParam {
    public:
    T value, min, max, step;

    inline TunableParam(T value, T min, T max, T step) 
    : value(value), min(min), max(max), step(step) { }

    // overload function operator
    inline T operator () () const {
        return value;
    }
};

// Eval scale with material / game phase
TunableParam<float> evalMaterialScaleMin = TunableParam<float>(0.78, 0.5, 1.0, 0.1);
TunableParam<float> evalMaterialScaleMax = TunableParam<float>(1.05, 1.0, 1.5, 0.1);

// Time management
TunableParam<u64> defaultMovesToGo = TunableParam<u64>(20, 20, 26, 3);
TunableParam<double> hardTimePercentage = TunableParam<double>(0.54, 0.3, 0.6, 0.15);
TunableParam<double> softTimePercentage = TunableParam<double>(0.7, 0.5, 0.7, 0.1);

// Nodes time management (scale soft time based on best move nodes fraction)
TunableParam<i32> nodesTmMinDepth = TunableParam<i32>(10, 7, 11, 1);
TunableParam<double> nodesTmBase = TunableParam<double>(1.59, 1.4, 1.6, 0.1);
TunableParam<double> nodesTmMultiplier = TunableParam<double>(1.47, 1.3, 1.5, 0.1);

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>(7, 6, 10, 1);
TunableParam<i32> aspInitialDelta = TunableParam<i32>(16, 5, 25, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>(1.42, 1.2, 2.0, 0.1);

// RFP (Reverse futility pruning)
TunableParam<i32> rfpMaxDepth = TunableParam<i32>(8, 6, 10, 1);
TunableParam<i32> rfpMultiplier = TunableParam<i32>(82, 40, 130, 10);

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>(5, 2, 6, 2);
TunableParam<i32> razoringMultiplier = TunableParam<i32>(317, 250, 450, 50);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>(3, 2, 4, 1);
TunableParam<i32> nmpBaseReduction = TunableParam<i32>(3, 2, 4, 1);
TunableParam<float> nmpReductionDivisor = TunableParam<float>(3.35, 2.0, 4.0, 0.5);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>(251, 100, 300, 50);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>(2, 1, 5, 2);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>(5, 4, 6, 1);

// LMP (Late move pruning)
TunableParam<i32> lmpMinMoves = TunableParam<i32>(3, 2, 4, 1);
TunableParam<float> lmpMultiplier = TunableParam<float>(1.07, 0.5, 1.5, 0.1);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>(8, 6, 10, 1);
TunableParam<i32> fpBase = TunableParam<i32>(240, 40, 260, 20);
TunableParam<i32> fpMultiplier = TunableParam<i32>(181, 40, 260, 20);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>(10, 7, 11, 1);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>(-60, -101, -1, 20);
TunableParam<i32> seeQuietHistoryDiv = TunableParam<i32>(174, 32, 512, 48);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>(-21, -51, -1, 10);
TunableParam<i32> seeNoisyHistoryDiv = TunableParam<i32>(152, 32, 512, 48);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>(7, 6, 10, 1);
TunableParam<i32> singularDepthMargin = TunableParam<i32>(3, 1, 5, 1);
TunableParam<i32> singularBetaMultiplier = TunableParam<i32>(1, 1, 3, 1);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>(35, 2, 42, 10);
const u8 DOUBLE_EXTENSIONS_MAX = 10;

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>(0.72, 0.4, 1.2, 0.1);
TunableParam<double> lmrMultiplier = TunableParam<double>(0.36, 0.2, 0.8, 0.1);
TunableParam<i32> lmrMinMoves = TunableParam<i32>(2, 2, 4, 1);
TunableParam<i32> lmrQuietHistoryDiv = TunableParam<i32>(5878, 1024, 16384, 1024);
TunableParam<i32> lmrNoisyHistoryDiv = TunableParam<i32>(2235, 1024, 16384, 1024);

// History max
TunableParam<i32> historyMax = TunableParam<i32>(8254, 8192, 24576, 2048);

// History bonus
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>(194, 50, 600, 50);
TunableParam<i32> historyBonusOffset = TunableParam<i32>(240, 0, 500, 100);
TunableParam<i32> historyBonusMax = TunableParam<i32>(1570, 500, 2500, 200);

// History malus
TunableParam<i32> historyMalusMultiplier = TunableParam<i32>(369, 50, 600, 50);
TunableParam<i32> historyMalusOffset = TunableParam<i32>(136, 0, 500, 100);
TunableParam<i32> historyMalusMax = TunableParam<i32>(1430, 500, 2500, 200);

// History weights
TunableParam<float> mainHistoryWeight = TunableParam<float>(0.35, 0.25, 4.0, 0.25);
TunableParam<float> onePlyContHistWeight = TunableParam<float>(1.42, 0.25, 4.0, 0.25);
TunableParam<float> twoPlyContHistWeight = TunableParam<float>(1.07, 0.25, 4.0, 0.25);
TunableParam<float> fourPlyContHistWeight = TunableParam<float>(0.42, 0.25, 4.0, 0.25);

// SEE piece values
TunableParam<i32> seePawnValue = TunableParam<i32>(167, 0, 200, 50);
TunableParam<i32> seeMinorValue = TunableParam<i32>(222, 150, 450, 50);
TunableParam<i32> seeRookValue = TunableParam<i32>(397, 300, 700, 50);
TunableParam<i32> seeQueenValue = TunableParam<i32>(729 , 600, 1200, 100);

using TunableParamVariant = std::variant<
    TunableParam<u64>*,
    TunableParam<i32>*, 
    TunableParam<double>*, 
    TunableParam<float>*
>;

tsl::ordered_map<std::string, TunableParamVariant> tunableParams = {
    // Eval scale with material / game phase
    {stringify(evalMaterialScaleMin), &evalMaterialScaleMin},
    {stringify(evalMaterialScaleMax), &evalMaterialScaleMax},

    // Time management
    {stringify(defaultMovesToGo), &defaultMovesToGo},
    {stringify(hardTimePercentage), &hardTimePercentage},
    {stringify(softTimePercentage), &softTimePercentage},

    // Nodes time management (scale soft time based on best move nodes fraction)
    {stringify(nodesTmMinDepth), &nodesTmMinDepth},
    {stringify(nodesTmBase), &nodesTmBase},
    {stringify(nodesTmMultiplier), &nodesTmMultiplier},

    // Aspiration windows
    {stringify(aspMinDepth), &aspMinDepth},
    {stringify(aspInitialDelta), &aspInitialDelta},
    {stringify(aspDeltaMultiplier), &aspDeltaMultiplier},

    // RFP (Reverse futility pruning)
    {stringify(rfpMaxDepth), &rfpMaxDepth},
    {stringify(rfpMultiplier), &rfpMultiplier},

    // Razoring
    {stringify(razoringMaxDepth), &razoringMaxDepth},
    {stringify(razoringMultiplier), &razoringMultiplier},

    // NMP (Null move pruning)
    {stringify(nmpMinDepth), &nmpMinDepth},
    {stringify(nmpBaseReduction), &nmpBaseReduction},
    {stringify(nmpReductionDivisor), &nmpReductionDivisor},
    {stringify(nmpEvalBetaDivisor), &nmpEvalBetaDivisor},
    {stringify(nmpEvalBetaMax), &nmpEvalBetaMax},

    // IIR (Internal iterative reduction)
    {stringify(iirMinDepth), &iirMinDepth},

    // LMP (Late move pruning)
    {stringify(lmpMinMoves), &lmpMinMoves},
    {stringify(lmpMultiplier), &lmpMultiplier},

    // FP (Futility pruning)
    {stringify(fpMaxDepth), &fpMaxDepth},
    {stringify(fpBase), &fpBase},
    {stringify(fpMultiplier), &fpMultiplier},

    // SEE pruning
    {stringify(seePruningMaxDepth), &seePruningMaxDepth},
    {stringify(seeQuietThreshold), &seeQuietThreshold},
    {stringify(seeQuietHistoryDiv), &seeQuietHistoryDiv},
    {stringify(seeNoisyThreshold), &seeNoisyThreshold},
    {stringify(seeNoisyHistoryDiv), &seeNoisyHistoryDiv},

    // SE (Singular extensions)
    {stringify(singularMinDepth), &singularMinDepth},
    {stringify(singularDepthMargin), &singularDepthMargin},
    {stringify(singularBetaMultiplier), &singularBetaMultiplier},
    {stringify(doubleExtensionMargin), &doubleExtensionMargin},

    // LMR (Late move reductions)
    {stringify(lmrBase), &lmrBase},
    {stringify(lmrMultiplier), &lmrMultiplier},
    {stringify(lmrMinMoves), &lmrMinMoves},
    {stringify(lmrQuietHistoryDiv), &lmrQuietHistoryDiv},
    {stringify(lmrNoisyHistoryDiv), &lmrNoisyHistoryDiv},

    // History max
    {stringify(historyMax), &historyMax},

    // History bonus
    {stringify(historyBonusMultiplier), &historyBonusMultiplier},
    {stringify(historyBonusOffset), &historyBonusOffset},
    {stringify(historyBonusMax), &historyBonusMax},

    // History malus
    {stringify(historyMalusMultiplier), &historyMalusMultiplier},
    {stringify(historyMalusOffset), &historyMalusOffset},
    {stringify(historyMalusMax), &historyMalusMax},

    // History weights
    {stringify(mainHistoryWeight), &mainHistoryWeight},
    {stringify(onePlyContHistWeight), &onePlyContHistWeight},
    {stringify(twoPlyContHistWeight), &twoPlyContHistWeight},
    {stringify(fourPlyContHistWeight), &fourPlyContHistWeight},

    // SEE piece values
    {stringify(seePawnValue), &seePawnValue},
    {stringify(seeMinorValue), &seeMinorValue},
    {stringify(seeRookValue), &seeRookValue},
    {stringify(seeQueenValue), &seeQueenValue}
};

inline void printParamsAsJson() {
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

    for (auto [paramName, tunableParam] : tunableParams) {
        std::cout << indent << "\"" << paramName << "\"" << ": {" << std::endl;

        std::visit([&] (auto *myParam) 
        {
            if (myParam == nullptr) return;

            std::cout << indent << indent 
                      << "\"value\": " 
                      << (std::is_same<decltype(myParam->value), double>::value
                         || std::is_same<decltype(myParam->value), float>::value
                         ? round(myParam->value * 100.0)
                         : myParam->value)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"min_value\": " 
                      << (std::is_same<decltype(myParam->min), double>::value
                         || std::is_same<decltype(myParam->min), float>::value
                         ? round(myParam->min * 100.0)
                         : myParam->min)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"max_value\": " 
                      << (std::is_same<decltype(myParam->max), double>::value
                         || std::is_same<decltype(myParam->max), float>::value
                         ? round(myParam->max * 100.0)
                         : myParam->max)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"step\": " 
                      << (std::is_same<decltype(myParam->step), double>::value
                         || std::is_same<decltype(myParam->step), float>::value
                         ? round(myParam->step * 100.0)
                         : myParam->step)
                      << std::endl;

            std::cout << indent << "}," << std::endl;
        }, tunableParam);
    }

    std::cout << "}" << std::endl;
}


