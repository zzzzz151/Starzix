#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max, step;

    inline TunableParam(const std::string name, T value, T min, T max, T step) 
    : name(name), value(value), min(min), max(max), step(step) { }
};

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>("aspMinDepth", 8, 6, 10, 2);
TunableParam<i32> aspInitialDelta = TunableParam<i32>("aspInitialDelta", 11, 10, 20, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.28, 1.25, 2.0, 0.25);

// RFP (Reverse futility pruning) / Static NMP
TunableParam<i32> rfpMaxDepth = TunableParam<i32>("rfpMaxDepth", 6, 6, 10, 2);
TunableParam<i32> rfpDepthMultiplier = TunableParam<i32>("rfpDepthMultiplier", 61, 40, 160, 60);

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>("razoringMaxDepth", 3, 2, 6, 2);
TunableParam<i32> razoringDepthMultiplier = TunableParam<i32>("razoringDepthMultiplier", 386, 250, 450, 100);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>("nmpMinDepth", 2, 2, 4, 1);
TunableParam<i32> nmpBaseReduction = TunableParam<i32>("nmpBaseReduction", 4, 2, 4, 1);
TunableParam<i32> nmpReductionDivisor = TunableParam<i32>("nmpReductionDivisor", 3, 2, 4, 1);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>("nmpEvalBetaDivisor", 274, 100, 300, 100);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>("nmpEvalBetaMax", 3, 2, 4, 2);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>("iirMinDepth", 5, 4, 6, 2);

// LMP (Late move pruning)
TunableParam<i32> lmpMinMoves = TunableParam<i32>("lmpMinMoves", 2, 2, 4, 2);
TunableParam<double> lmpDepthMultiplier = TunableParam<double>("lmpDepthMultiplier", 1.0, 0.5, 1.5, 0.5);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>("fpMaxDepth", 7, 6, 10, 2);
TunableParam<i32> fpBase = TunableParam<i32>("fpBase", 97, 50, 200, 50);
TunableParam<i32> fpMultiplier = TunableParam<i32>("fpMultiplier", 136, 60, 180, 60);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>("seePruningMaxDepth", 8, 7, 11, 2);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>("seeNoisyThreshold", -17, -35, -5, 15);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>("seeQuietThreshold", -60, -85, -45, 20);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>("singularMinDepth", 6, 6, 10, 2);
TunableParam<i32> singularDepthMargin = TunableParam<i32>("singularDepthMargin", 4, 1, 5, 2);
TunableParam<i32> singularBetaMultiplier = TunableParam<i32>("singularBetaMultiplier", 2, 1, 3, 1);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>("doubleExtensionMargin", 23, 10, 30, 10);
TunableParam<u8> doubleExtensionsMax = TunableParam<u8>("doubleExtensionsMax", 5, 2, 8, 3);

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.69, 0.6, 1.0, 0.2);
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.53, 0.3, 0.6, 0.15);
TunableParam<i32> lmrQuietHistoryDiv = TunableParam<i32>("lmrQuietHistoryDiv", 9695, 4096, 24576, 4096);
TunableParam<i32> lmrNoisyHistoryDiv = TunableParam<i32>("lmrNoisyHistoryDiv", 2407, 4, 16384, 4095);

// History
TunableParam<i32> historyMaxBonus = TunableParam<i32>("historyMaxBonus", 1320, 1200, 1800, 300);
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>("historyBonusMultiplier", 292, 200, 400, 100);
TunableParam<i32> historyMax = TunableParam<i32>("historyMax", 12042, 8192, 24576, 4096);

// Time Management
TunableParam<u64> defaultMovesToGo = TunableParam<u64>("defaultMovesToGo", 20, 20, 25, 5);
TunableParam<double> hardTimePercentage = TunableParam<double>("hardTimePercentage", 0.45, 0.3, 0.6, 0.15);
TunableParam<double> softTimePercentage = TunableParam<double>("softTimePercentage", 0.6, 0.5, 0.7, 0.2);

// Nodes time management (scale soft time based on best move nodes fraction)
TunableParam<i32> nodesTmMinDepth = TunableParam<i32>("nodesTmMinDepth", 9, 7, 11, 2);
TunableParam<double> nodesTmBase = TunableParam<double>("nodesTmBase", 1.57, 1.4, 1.6, 0.2);
TunableParam<double> nodesTmMultiplier = TunableParam<double>("nodesTmMultiplier", 1.47, 1.3, 1.5, 0.2);

using TunableParamVariant = std::variant<TunableParam<i32>*, TunableParam<double>*, TunableParam<u16>*, TunableParam<u8>*, TunableParam<u64>*>;

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
    &lmrBase, &lmrMultiplier, &lmrQuietHistoryDiv,  &lmrNoisyHistoryDiv,
    &historyMaxBonus, &historyBonusMultiplier, &historyMax,
    &defaultMovesToGo, &hardTimePercentage, &softTimePercentage,
    &nodesTmMinDepth, &nodesTmBase, &nodesTmMultiplier
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


