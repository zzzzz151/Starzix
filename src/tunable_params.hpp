#include <variant>

template <typename T> struct TunableParam {

    const std::string name;
    T value, min, max;

    inline TunableParam(const std::string name, T value, T min, T max) 
    : name(name), value(value), min(min), max(max) { }
};

namespace search {

// Aspiration windows
TunableParam<u8> aspMinDepth = TunableParam<u8>("aspMinDepth", 8, 6, 10); // step = 2
TunableParam<u8> aspInitialDelta = TunableParam<u8>("aspInitialDelta", 15, 10, 20); // step = 5
TunableParam<double> aspDeltaMultiplier = TunableParam<double>("aspDeltaMultiplier", 1.4, 1.2, 2.0); // step = 0.2

// RFP (Reverse futility pruning) / Static NMP
TunableParam<u8> rfpMaxDepth = TunableParam<u8>("rfpMaxDepth", 8, 6, 10); // step = 2
TunableParam<u8> rfpDepthMultiplier = TunableParam<u8>("rfpDepthMultiplier", 75, 50, 150); // step = 25

// AP (Alpha pruning)
TunableParam<u8> apMaxDepth = TunableParam<u8>("apMaxDepth", 4, 2, 8); // step = 2
TunableParam<u16> apMargin = TunableParam<u16>("apMargin", 2000, 1000, 5000); // step = 500

// Razoring
TunableParam<u8> razoringMaxDepth = TunableParam<u8>("razoringMaxDepth", 6, 2, 6); // step = 2
TunableParam<u16> razoringDepthMultiplier = TunableParam<u16>("razoringDepthMultiplier", 300, 200, 400); // step = 100

// NMP (Null move pruning)
TunableParam<u8> nmpMinDepth = TunableParam<u8>("nmpMinDepth", 3, 2, 4); // step = 1
TunableParam<u8> nmpBaseReduction = TunableParam<u8>("nmpBaseReduction", 3, 2, 4); // step = 1
TunableParam<u8> nmpReductionDivisor = TunableParam<u8>("nmpReductionDivisor", 3, 2, 4); // step = 1

// IIR (Internal iterative reduction)
TunableParam<u8> iirMinDepth = TunableParam<u8>("iirMinDepth", 4, 4, 6); // step = 2

// LMP (Late move pruning)
TunableParam<u8> lmpMaxDepth = TunableParam<u8>("lmpMaxDepth", 8, 6, 12); // step = 2
TunableParam<u8> lmpMinMoves = TunableParam<u8>("lmpMinMoves", 3, 2, 4); // step = 1
TunableParam<double> lmpDepthMultiplier = TunableParam<double>("lmpDepthMultiplier", 0.75, 0.5, 1.5); // step = 0.25

// FP (Futility pruning)
TunableParam<u8> fpMaxDepth = TunableParam<u8>("fpMaxDepth", 8, 6, 10); // step = 2
TunableParam<u8> fpBase = TunableParam<u8>("fpBase", 120, 60, 180); // step = 30
TunableParam<u8> fpMultiplier = TunableParam<u8>("fpMultiplier", 70, 50, 150); // step = 20

// SEE pruning
TunableParam<u8> seePruningMaxDepth = TunableParam<u8>("seePruningMaxDepth", 9, 7, 11); // step = 2
TunableParam<i8> seeNoisyThreshold = TunableParam<i8>("seeNoisyThreshold", -20, -30, -10); // step = 10
TunableParam<i8> seeQuietThreshold = TunableParam<i8>("seeQuietThreshold", -65, -80, -50); // step = 15

// SE (Singular extensions)
TunableParam<u8> singularMinDepth = TunableParam<u8>("singularMinDepth", 8, 6, 10); // step = 2
TunableParam<u8> singularDepthMargin = TunableParam<u8>("singularDepthMargin", 3, 1, 5); // step = 2
TunableParam<u8> singularBetaMultiplier = TunableParam<u8>("singularBetaMultiplier", 2, 1, 3); // step = 1
TunableParam<u8> singularBetaMargin = TunableParam<u8>("singularBetaMargin", 24, 12, 48); // step = 12
TunableParam<u8> maxDoubleExtensions = TunableParam<u8>("maxDoubleExtensions", 7, 4, 10); // step = 3

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>("lmrBase", 0.8, 0.6, 1.0); // step = 0.1
TunableParam<double> lmrMultiplier = TunableParam<double>("lmrMultiplier", 0.4, 0.3, 0.6); // step = 0.1
TunableParam<u16> lmrHistoryDivisor = TunableParam<u16>("lmrHistoryDivisor", 8192, 4096, 16384); // step = 4096
TunableParam<u16> lmrNoisyHistoryDivisor = TunableParam<u16>("lmrNoisyHistoryDivisor", 4096, 2048, 8192); // step = 2048

// History
TunableParam<u16> historyMinBonus = TunableParam<u16>("historyMinBonus", 1550, 1300, 1800); // step = 250
TunableParam<u16> historyBonusMultiplier = TunableParam<u16>("historyBonusMultiplier", 370, 270, 470); // step = 100
TunableParam<u16> historyMax = TunableParam<u16>("historyMax", 16384, 16384, 32768); // step = 16384

// Time management
TunableParam<double> suddenDeathHardTimePercentage = TunableParam<double>("suddenDeathHardTimePercentage", 0.5, 0.35, 0.65); // step = 0.15
TunableParam<double> suddenDeathSoftTimePercentage = TunableParam<double>("suddenDeathSoftTimePercentage", 0.05, 0.03, 0.07); // step = 0.02
TunableParam<double> movesToGoHardTimePercentage = TunableParam<double>("movesToGoHardTimePercentage", 0.5, 0.3, 0.7); // step = 0.2
TunableParam<double> movesToGoSoftTimePercentage = TunableParam<double>("movesToGoSoftTimePercentage", 0.6, 0.5, 0.6); // step = 0.1
TunableParam<double> softTimeScaleBase = TunableParam<double>("softTimeScaleBase", 0.25, 0.25, 0.75); // step = 0.25
TunableParam<double> softTimeScaleMultiplier = TunableParam<double>("softTimeScaleMultiplier", 1.75, 1.25, 1.75); // step = 0.25

using TunableParamVariant = std::variant<TunableParam<u8>*, TunableParam<u16>*, TunableParam<i8>*, TunableParam<double>*>;
std::vector<TunableParamVariant> tunableParams {
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
    &historyMinBonus, &historyBonusMultiplier, &historyMax,
    &suddenDeathHardTimePercentage, &suddenDeathSoftTimePercentage,
    &movesToGoHardTimePercentage, &movesToGoSoftTimePercentage,
    &softTimeScaleBase, &softTimeScaleMultiplier
};

}






