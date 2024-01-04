#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max;

    inline TunableParam(const std::string name, T value, T min, T max) 
    : name(name), value(value), min(min), max(max) { }
};

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>("aspMinDepth", 7, 6, 10); // step = 2
TunableParam<i32> aspInitialDelta = TunableParam<i32>("aspInitialDelta", 10, 10, 20); // step = 5
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.32, 1.2, 2.0); // step = 0.2

// RFP (Reverse futility pruning) / Static NMP
TunableParam<i32> rfpMaxDepth = TunableParam<i32>("rfpMaxDepth", 7, 6, 10); // step = 2
TunableParam<i32> rfpDepthMultiplier = TunableParam<i32>("rfpDepthMultiplier", 75, 50, 150); // step = 25

// AP (Alpha pruning)
TunableParam<i32> apMaxDepth = TunableParam<i32>("apMaxDepth", 6, 2, 8); // step = 2
TunableParam<i32> apMargin = TunableParam<i32>("apMargin", 1815, 1000, 5000); // step = 500

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>("razoringMaxDepth", 4, 2, 6); // step = 2
TunableParam<i32> razoringDepthMultiplier = TunableParam<i32>("razoringDepthMultiplier", 297, 200, 400); // step = 100

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>("nmpMinDepth", 3, 2, 4); // step = 1
TunableParam<i32> nmpBaseReduction = TunableParam<i32>("nmpBaseReduction", 3, 2, 4); // step = 1
TunableParam<i32> nmpReductionDivisor = TunableParam<i32>("nmpReductionDivisor", 3, 2, 4); // step = 1

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>("iirMinDepth", 4, 4, 6); // step = 2

// LMP (Late move pruning)
TunableParam<i32> lmpMaxDepth = TunableParam<i32>("lmpMaxDepth", 8, 6, 12); // step = 2
TunableParam<i32> lmpMinMoves = TunableParam<i32>("lmpMinMoves", 3, 2, 4); // step = 1
TunableParam<double> lmpDepthMultiplier = TunableParam<double>("lmpDepthMultiplier", 0.75, 0.5, 1.5); // step = 0.25

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>("fpMaxDepth", 9, 6, 10); // step = 2
TunableParam<i32> fpBase = TunableParam<i32>("fpBase", 124, 60, 180); // step = 30
TunableParam<i32> fpMultiplier = TunableParam<i32>("fpMultiplier", 82, 50, 150); // step = 20

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>("seePruningMaxDepth", 9, 7, 11); // step = 2
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>("seeNoisyThreshold", -20, -30, -10); // step = 10
TunableParam<i32> seeQuietThreshold = TunableParam<i32>("seeQuietThreshold", -65, -80, -50); // step = 15

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>("singularMinDepth", 7, 6, 10); // step = 2
TunableParam<i32> singularDepthMargin = TunableParam<i32>("singularDepthMargin", 3, 1, 5); // step = 2
TunableParam<i32> singularBetaMultiplier = TunableParam<i32>("singularBetaMultiplier", 2, 1, 3); // step = 1
TunableParam<i32> singularBetaMargin = TunableParam<i32>("singularBetaMargin", 23, 12, 48); // step = 12
TunableParam<i32> maxDoubleExtensions = TunableParam<i32>("maxDoubleExtensions", 6, 4, 10); // step = 3

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.8, 0.6, 1.0); // step = 0.1
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.4, 0.3, 0.6); // step = 0.1
TunableParam<i32> lmrHistoryDivisor = TunableParam<i32>("lmrHistoryDivisor", 8192, 4096, 16384); // step = 4096
TunableParam<i32> lmrNoisyHistoryDivisor = TunableParam<i32>("lmrNoisyHistoryDivisor", 6689, 2048, 8192); // step = 2048

// History
TunableParam<i32> historyMaxBonus = TunableParam<i32>("historyMaxBonus", 1570, 1300, 1800); // step = 250
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>("historyBonusMultiplier", 370, 270, 470); // step = 100
TunableParam<i32> historyMax = TunableParam<i32>("historyMax", 16384, 16384, 32768); // step = 16384

// Time Management
TunableParam<u16> defaultMovesToGo = TunableParam<u16>("defaultMovesToGo", 25, 20, 30); // step = 5
TunableParam<double> softMillisecondsPercentage = TunableParam<double>("softMillisecondsPercentage", 0.6, 0.5, 0.6); // step = 0.1
TunableParam<i32> softTimeMoveNodesScalingMinDepth = TunableParam<i32>("softTimeMoveNodesScalingMinDepth", 9, 7, 11); // step = 2
TunableParam<double> softTimeMoveNodesScalingBase = TunableParam<double>("softTimeMoveNodesScalingBase", 1.5, 1.25, 1.75); // step = 0.25
TunableParam<double> softTimeMoveNodesScalingMultiplier = TunableParam<double>("softTimeMoveNodesScalingMultiplier", 1.35, 1.15, 1.55); // step = 0.2

using TunableParamVariant = std::variant<TunableParam<i32>*, TunableParam<double>*, TunableParam<u16>*>;

std::vector<TunableParamVariant> tunableParams 
{
    &aspMinDepth, &aspInitialDelta, &aspDeltaMultiplier,
    &rfpMaxDepth, &rfpDepthMultiplier,
    &apMaxDepth, &apMargin,
    &razoringMaxDepth, &razoringDepthMultiplier,
    &nmpMinDepth, &nmpBaseReduction, &nmpReductionDivisor,
    &iirMinDepth,
    &lmpMaxDepth, &lmpMinMoves, &lmpDepthMultiplier,
    &fpMaxDepth, &fpBase, &fpMultiplier,
    &seePruningMaxDepth, &seeNoisyThreshold, &seeQuietThreshold,
    &singularMinDepth, &singularDepthMargin, &singularBetaMultiplier, &singularBetaMargin, &maxDoubleExtensions,
    &lmrBase, &lmrMultiplier, &lmrHistoryDivisor, &lmrNoisyHistoryDivisor,
    &historyMaxBonus, &historyBonusMultiplier, &historyMax,
    &defaultMovesToGo, &softMillisecondsPercentage,
    &softTimeMoveNodesScalingMinDepth, &softTimeMoveNodesScalingBase, &softTimeMoveNodesScalingMultiplier
};

