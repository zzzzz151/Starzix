#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max, step;

    inline TunableParam(const std::string name, T value, T min, T max, T step) 
    : name(name), value(value), min(min), max(max), step(step) { }
};

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>("aspMinDepth", 8, 6, 10, 2);
TunableParam<i32> aspInitialDelta = TunableParam<i32>("aspInitialDelta", 16, 10, 20, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.44, 1.25, 2.0, 0.25);

// RFP (Reverse futility pruning) / Static NMP
TunableParam<i32> rfpMaxDepth = TunableParam<i32>("rfpMaxDepth", 7, 6, 10, 2);
TunableParam<i32> rfpDepthMultiplier = TunableParam<i32>("rfpDepthMultiplier", 73, 50, 150, 50);

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>("razoringMaxDepth", 2, 2, 6, 2);
TunableParam<i32> razoringDepthMultiplier = TunableParam<i32>("razoringDepthMultiplier", 420, 250, 450, 100);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>("nmpMinDepth", 2, 2, 4, 2);
TunableParam<i32> nmpBaseReduction = TunableParam<i32>("nmpBaseReduction", 3, 2, 4, 1);
TunableParam<i32> nmpReductionDivisor = TunableParam<i32>("nmpReductionDivisor", 3, 2, 4, 1);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>("nmpEvalBetaDivisor", 150, 100, 300, 100);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>("nmpEvalBetaMax", 4, 2, 4, 2);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>("iirMinDepth", 5, 4, 6, 2);

// LMP (Late move pruning)
TunableParam<i32> lmpMinMoves = TunableParam<i32>("lmpMinMoves", 2, 2, 4, 2);
TunableParam<double> lmpDepthMultiplier = TunableParam<double>("lmpDepthMultiplier", 0.92, 0.5, 1.5, 0.5);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>("fpMaxDepth", 6, 6, 10, 2);
TunableParam<i32> fpBase = TunableParam<i32>("fpBase", 155, 75, 225, 50);
TunableParam<i32> fpMultiplier = TunableParam<i32>("fpMultiplier", 143, 60, 180, 60);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>("seePruningMaxDepth", 11, 7, 11, 2);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>("seeNoisyThreshold", -14, -35, -5, 15);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>("seeQuietThreshold", -58, -85, -45, 20);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>("singularMinDepth", 7, 6, 10, 2);
TunableParam<i32> singularDepthMargin = TunableParam<i32>("singularDepthMargin", 3, 1, 5, 2);
TunableParam<i32> singularBetaMultiplier = TunableParam<i32>("singularBetaMultiplier", 2, 1, 3, 1);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>("doubleExtensionMargin", 18, 10, 30, 10);
TunableParam<u8> doubleExtensionsMax = TunableParam<u8>("doubleExtensionsMax", 7, 2, 8, 3);

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.69, 0.6, 1.0, 0.2);
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.48, 0.3, 0.6, 0.15);
TunableParam<i32> lmrQuietHistoryDiv = TunableParam<i32>("lmrQuietHistoryDiv", 8192, 4096, 32768, 4096);
TunableParam<i32> lmrNoisyHistoryDiv = TunableParam<i32>("lmrNoisyHistoryDiv", 4096, 0, 32768, 8192);

// History
TunableParam<i32> historyMaxBonus = TunableParam<i32>("historyMaxBonus", 1800, 1200, 2400, 300);
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>("historyBonusMultiplier", 300, 200, 400, 100);
TunableParam<i32> historyMax = TunableParam<i32>("historyMax", 16384, 8192, 32768, 4096);

// Time Management
TunableParam<u16> defaultMovesToGo = TunableParam<u16>("defaultMovesToGo", 23, 20, 25, 5);
TunableParam<double> softMillisecondsPercentage = TunableParam<double>("softMillisecondsPercentage", 0.6, 0.5, 0.6, 0.1);

// Nodes time management (scale soft time based on best move nodes fraction)
TunableParam<i32> nodesTmMinDepth = TunableParam<i32>("nodesTmMinDepth", 10, 7, 11, 2);
TunableParam<double> nodesTmBase = TunableParam<double>("nodesTmBase", 1.55, 1.4, 1.6, 0.2);
TunableParam<double> nodesTmMultiplier = TunableParam<double>("nodesTmMultiplier", 1.4, 1.3, 1.5, 0.2);

using TunableParamVariant = std::variant<TunableParam<i32>*, TunableParam<double>*, TunableParam<u16>*, TunableParam<u8>*>;

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
    &defaultMovesToGo, &softMillisecondsPercentage,
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


