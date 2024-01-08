#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max, step;

    inline TunableParam(const std::string name, T value, T min, T max, T step) 
    : name(name), value(value), min(min), max(max), step(step) { }
};

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>("aspMinDepth", 7, 6, 10, 2);
TunableParam<i32> aspInitialDelta = TunableParam<i32>("aspInitialDelta", 15, 10, 20, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.5, 1.25, 2.0, 0.25);

// RFP (Reverse futility pruning) / Static NMP
TunableParam<i32> rfpMaxDepth = TunableParam<i32>("rfpMaxDepth", 7, 6, 10, 2);
TunableParam<i32> rfpDepthMultiplier = TunableParam<i32>("rfpDepthMultiplier", 75, 50, 150, 50);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>("nmpMinDepth", 3, 2, 4, 1);
TunableParam<i32> nmpBaseReduction = TunableParam<i32>("nmpBaseReduction", 3, 2, 4, 1);
TunableParam<i32> nmpReductionDivisor = TunableParam<i32>("nmpReductionDivisor", 3, 2, 4, 1);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>("nmpEvalBetaDivisor", 200, 150, 250, 50);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>("nmpEvalBetaMax", 3, 2, 4, 1);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>("iirMinDepth", 4, 4, 6, 2);

// LMP (Late move pruning)
TunableParam<i32> lmpMaxDepth = TunableParam<i32>("lmpMaxDepth", 8, 6, 12, 2);
TunableParam<i32> lmpMinMoves = TunableParam<i32>("lmpMinMoves", 3, 2, 4, 1);
TunableParam<double> lmpDepthMultiplier = TunableParam<double>("lmpDepthMultiplier", 0.75, 0.5, 1.5, 0.5);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>("fpMaxDepth", 8, 6, 10, 2);
TunableParam<i32> fpBase = TunableParam<i32>("fpBase", 180, 75, 225, 50);
TunableParam<i32> fpMultiplier = TunableParam<i32>("fpMultiplier", 120, 60, 180, 60);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>("seePruningMaxDepth", 9, 7, 11, 2);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>("seeNoisyThreshold", -20, -35, -5, 15);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>("seeQuietThreshold", -65, -85, -45, 20);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>("singularMinDepth", 8, 6, 10, 2);
TunableParam<i32> singularDepthMargin = TunableParam<i32>("singularDepthMargin", 3, 1, 5, 2);
TunableParam<i32> singularBetaMultiplier = TunableParam<i32>("singularBetaMultiplier", 2, 1, 3, 1);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>("doubleExtensionMargin", 20, 12, 32, 10);
TunableParam<u8> doubleExtensionsMax = TunableParam<u8>("doubleExtensionsMax", 5, 2, 8, 3);

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.8, 0.6, 1.0, 0.2);
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.4, 0.3, 0.6, 0.15);
TunableParam<i32> lmrHistoryDivisor = TunableParam<i32>("lmrHistoryDivisor", 8192, 4096, 16384, 4096);

// History
TunableParam<i32> historyMaxBonus = TunableParam<i32>("historyMaxBonus", 1570, 1300, 1800, 250);
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>("historyBonusMultiplier", 370, 270, 470, 100);
TunableParam<i32> historyMax = TunableParam<i32>("historyMax", 16384, 16384, 16384, 0);

// Time Management
TunableParam<u16> defaultMovesToGo = TunableParam<u16>("defaultMovesToGo", 25, 20, 25, 5);
TunableParam<double> softMillisecondsPercentage = TunableParam<double>("softMillisecondsPercentage", 0.6, 0.5, 0.6, 0.1);
TunableParam<i32> softTimeMoveNodesScalingMinDepth = TunableParam<i32>("softTimeMoveNodesScalingMinDepth", 9, 7, 11, 2);
TunableParam<double> softTimeMoveNodesScalingBase = TunableParam<double>("softTimeMoveNodesScalingBase", 1.5, 1.25, 1.75, 0.5);
TunableParam<double> softTimeMoveNodesScalingMultiplier = TunableParam<double>("softTimeMoveNodesScalingMultiplier", 1.35, 1.1, 1.6, 0.25);

using TunableParamVariant = std::variant<TunableParam<i32>*, TunableParam<double>*, TunableParam<u16>*, TunableParam<u8>*>;

std::vector<TunableParamVariant> tunableParams 
{
    &aspMinDepth, &aspInitialDelta, &aspDeltaMultiplier,
    &rfpMaxDepth, &rfpDepthMultiplier,
    &nmpMinDepth, &nmpBaseReduction, &nmpReductionDivisor, &nmpEvalBetaDivisor, &nmpEvalBetaMax,
    &iirMinDepth, 
    &lmpMaxDepth, &lmpMinMoves, &lmpDepthMultiplier,
    &fpMaxDepth, &fpBase, &fpMultiplier,
    &seePruningMaxDepth, &seeNoisyThreshold, &seeQuietThreshold,
    &singularMinDepth, &singularDepthMargin, &singularBetaMultiplier,
    &doubleExtensionMargin, &doubleExtensionsMax,
    &lmrBase, &lmrMultiplier, &lmrHistoryDivisor,
    &historyMaxBonus, &historyBonusMultiplier, &historyMax,
    &defaultMovesToGo, &softMillisecondsPercentage, &softTimeMoveNodesScalingMinDepth,
    &softTimeMoveNodesScalingBase, &softTimeMoveNodesScalingMultiplier
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
                          ? tunableParam->value * 100.0
                          : (i64)tunableParam->value)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"min_value\": " 
                      << (std::is_same<decltype(tunableParam->min), double>::value
                          ? tunableParam->min * 100.0
                          : (i64)tunableParam->min)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"max_value\": " 
                      << (std::is_same<decltype(tunableParam->max), double>::value
                          ? tunableParam->max * 100.0
                          : (i64)tunableParam->max)
                      << "," << std::endl;

            std::cout << indent << indent 
                      << "\"step\": " 
                      << (std::is_same<decltype(tunableParam->step), double>::value
                          ? tunableParam->step * 100.0
                          : (i64)tunableParam->step)
                      << std::endl;

            std::cout << indent << "}," << std::endl;

        }, myTunableParam);
    }

    std::cout << "}" << std::endl;
}

