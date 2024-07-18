// clang-format off

#pragma once

#include "3rdparty/ordered_map.h"
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

    inline bool floatOrDouble() {
        return std::is_same<decltype(value), double>::value
               || std::is_same<decltype(value), float>::value;
    }

};

constexpr u8 MAX_DEPTH = 100;

constexpr i32 GOOD_QUEEN_PROMO_SCORE = 1'600'000'000,
              GOOD_NOISY_SCORE       = 1'500'000'000,
              KILLER_SCORE           = 1'000'000'000,
              COUNTERMOVE_SCORE      = 500'000'000;

// Time management
TunableParam<double> hardTimePercentage = TunableParam<double>(0.5, 0.25, 0.75, 0.1);
TunableParam<double> softTimePercentage = TunableParam<double>(0.05, 0.01, 0.11, 0.02);

// Eval scale with material / game phase
TunableParam<float> evalMaterialScaleMin = TunableParam<float>(0.8, 0.5, 1.0, 0.1);
TunableParam<float> evalMaterialScaleMax = TunableParam<float>(1.1, 1.0, 1.5, 0.1);

// Aspiration windows
TunableParam<i32> aspMinDepth = TunableParam<i32>(6, 6, 10, 1);
TunableParam<i32> aspInitialDelta = TunableParam<i32>(20, 5, 25, 5);
TunableParam<double> aspDeltaMultiplier = TunableParam<double>(1.5, 1.2, 2.0, 0.1);

// Improving flag
TunableParam<i32> improvingThreshold = TunableParam<i32>(5, 1, 51, 5);

// RFP (Reverse futility pruning)
TunableParam<i32> rfpMaxDepth = TunableParam<i32>(8, 6, 10, 1);
TunableParam<i32> rfpMultiplier = TunableParam<i32>(75, 40, 130, 10);

// Razoring
TunableParam<i32> razoringMaxDepth = TunableParam<i32>(4, 2, 6, 2);
TunableParam<i32> razoringMultiplier = TunableParam<i32>(350, 250, 450, 50);

// NMP (Null move pruning)
TunableParam<i32> nmpMinDepth = TunableParam<i32>(3, 2, 4, 1);
TunableParam<i32> nmpBaseReduction = TunableParam<i32>(3, 2, 4, 1);
TunableParam<float> nmpReductionDivisor = TunableParam<float>(3.0, 2.0, 4.0, 0.5);
TunableParam<i32> nmpEvalBetaDivisor = TunableParam<i32>(200, 100, 300, 50);
TunableParam<i32> nmpEvalBetaMax = TunableParam<i32>(3, 1, 5, 2);

// IIR (Internal iterative reduction)
TunableParam<i32> iirMinDepth = TunableParam<i32>(4, 4, 6, 1);

// LMP (Late move pruning)
TunableParam<i32> lmpMinMoves = TunableParam<i32>(3, 2, 4, 1);
TunableParam<float> lmpMultiplier = TunableParam<float>(1.0, 0.5, 1.5, 0.1);

// FP (Futility pruning)
TunableParam<i32> fpMaxDepth = TunableParam<i32>(8, 6, 10, 1);
TunableParam<i32> fpBase = TunableParam<i32>(160, 40, 260, 20);
TunableParam<i32> fpMultiplier = TunableParam<i32>(160, 40, 260, 20);

// SEE pruning
TunableParam<i32> seePruningMaxDepth = TunableParam<i32>(8, 7, 11, 1);
TunableParam<i32> seeQuietThreshold = TunableParam<i32>(-60, -101, -1, 20);
TunableParam<i32> seeQuietHistoryDiv = TunableParam<i32>(256, 25, 525, 50);
TunableParam<i32> seeNoisyThreshold = TunableParam<i32>(-110, -161, -1, 20);
TunableParam<i32> seeNoisyHistoryDiv = TunableParam<i32>(128, 25, 525, 50);

// SE (Singular extensions)
TunableParam<i32> singularMinDepth = TunableParam<i32>(7, 6, 10, 1);
TunableParam<i32> singularDepthMargin = TunableParam<i32>(3, 1, 5, 1);
TunableParam<i32> doubleExtensionMargin = TunableParam<i32>(20, 2, 42, 10);
constexpr u8 DOUBLE_EXTENSIONS_MAX = 10;

// LMR (Late move reductions)
TunableParam<double> lmrBase = TunableParam<double>(0.8, 0.4, 1.2, 0.1);
TunableParam<double> lmrMultiplier = TunableParam<double>(0.4, 0.2, 0.8, 0.1);
TunableParam<i32> lmrMinMoves = TunableParam<i32>(3, 2, 4, 1);
TunableParam<i32> lmrHistoryDiv = TunableParam<i32>(32768, 8192, 32768, 2048);

// Deeper search in PVS with LMR
TunableParam<i32> deeperBase = TunableParam<i32>(50, 15, 95, 20);
TunableParam<i32> deeperMultiplier = TunableParam<i32>(2, 1, 3, 1);

// History max
TunableParam<i32> historyMax = TunableParam<i32>(16384, 8192, 24576, 2048);

// History bonus
TunableParam<i32> historyBonusMultiplier = TunableParam<i32>(300, 50, 600, 50);
TunableParam<i32> historyBonusOffset = TunableParam<i32>(50, 0, 500, 100);
TunableParam<i32> historyBonusMax = TunableParam<i32>(1500, 500, 2500, 200);

// History malus
TunableParam<i32> historyMalusMultiplier = TunableParam<i32>(300, 50, 600, 50);
TunableParam<i32> historyMalusOffset = TunableParam<i32>(50, 0, 500, 100);
TunableParam<i32> historyMalusMax = TunableParam<i32>(1500, 500, 2500, 200);

// History weights
TunableParam<float> mainHistoryWeight = TunableParam<float>(1.0, 0.25, 4.0, 0.25);
TunableParam<float> contHist1PlyWeight = TunableParam<float>(1.0, 0.25, 4.0, 0.25);
TunableParam<float> contHist2PlyWeight = TunableParam<float>(1.0, 0.25, 4.0, 0.25);
TunableParam<float> contHist4PlyWeight = TunableParam<float>(0.5, 0.25, 4.0, 0.25);

// Correction history
TunableParam<i32> corrHistScale = TunableParam<i32>(200, 50, 550, 50);
TunableParam<i32> corrHistMax = TunableParam<i32>(16384, 4096, 32768, 2048);

// SEE piece values
TunableParam<i32> seePawnValue = TunableParam<i32>(100, 0, 200, 50);
TunableParam<i32> seeMinorValue = TunableParam<i32>(300, 150, 450, 50);
TunableParam<i32> seeRookValue = TunableParam<i32>(500, 300, 700, 50);
TunableParam<i32> seeQueenValue = TunableParam<i32>(900 , 600, 1200, 100);

using TunableParamVariant = std::variant<
    TunableParam<u64>*,
    TunableParam<i32>*, 
    TunableParam<double>*, 
    TunableParam<float>*
>;

tsl::ordered_map<std::string, TunableParamVariant> tunableParams = {
    {stringify(hardTimePercentage), &hardTimePercentage},
    {stringify(softTimePercentage), &softTimePercentage},
    {stringify(evalMaterialScaleMin), &evalMaterialScaleMin},
    {stringify(evalMaterialScaleMax), &evalMaterialScaleMax},
    {stringify(aspMinDepth), &aspMinDepth},
    {stringify(aspInitialDelta), &aspInitialDelta},
    {stringify(aspDeltaMultiplier), &aspDeltaMultiplier},
    {stringify(improvingThreshold), &improvingThreshold},
    {stringify(rfpMaxDepth), &rfpMaxDepth},
    {stringify(rfpMultiplier), &rfpMultiplier},
    {stringify(razoringMaxDepth), &razoringMaxDepth},
    {stringify(razoringMultiplier), &razoringMultiplier},
    {stringify(nmpMinDepth), &nmpMinDepth},
    {stringify(nmpBaseReduction), &nmpBaseReduction},
    {stringify(nmpReductionDivisor), &nmpReductionDivisor},
    {stringify(nmpEvalBetaDivisor), &nmpEvalBetaDivisor},
    {stringify(nmpEvalBetaMax), &nmpEvalBetaMax},
    {stringify(iirMinDepth), &iirMinDepth},
    {stringify(lmpMinMoves), &lmpMinMoves},
    {stringify(lmpMultiplier), &lmpMultiplier},
    {stringify(fpMaxDepth), &fpMaxDepth},
    {stringify(fpBase), &fpBase},
    {stringify(fpMultiplier), &fpMultiplier},
    {stringify(seePruningMaxDepth), &seePruningMaxDepth},
    {stringify(seeQuietThreshold), &seeQuietThreshold},
    {stringify(seeQuietHistoryDiv), &seeQuietHistoryDiv},
    {stringify(seeNoisyThreshold), &seeNoisyThreshold},
    {stringify(seeNoisyHistoryDiv), &seeNoisyHistoryDiv},
    {stringify(singularMinDepth), &singularMinDepth},
    {stringify(singularDepthMargin), &singularDepthMargin},
    {stringify(doubleExtensionMargin), &doubleExtensionMargin},
    {stringify(lmrBase), &lmrBase},
    {stringify(lmrMultiplier), &lmrMultiplier},
    {stringify(lmrMinMoves), &lmrMinMoves},
    {stringify(lmrHistoryDiv), &lmrHistoryDiv},
    {stringify(deeperBase), &deeperBase},
    {stringify(deeperMultiplier), &deeperMultiplier},
    {stringify(historyMax), &historyMax},
    {stringify(historyBonusMultiplier), &historyBonusMultiplier},
    {stringify(historyBonusOffset), &historyBonusOffset},
    {stringify(historyBonusMax), &historyBonusMax},
    {stringify(historyMalusMultiplier), &historyMalusMultiplier},
    {stringify(historyMalusOffset), &historyMalusOffset},
    {stringify(historyMalusMax), &historyMalusMax},
    {stringify(mainHistoryWeight), &mainHistoryWeight},
    {stringify(contHist1PlyWeight), &contHist1PlyWeight},
    {stringify(contHist2PlyWeight), &contHist2PlyWeight},
    {stringify(contHist4PlyWeight), &contHist4PlyWeight},
    {stringify(corrHistScale), &corrHistScale},
    {stringify(corrHistMax), &corrHistMax},
    {stringify(seePawnValue), &seePawnValue},
    {stringify(seeMinorValue), &seeMinorValue},
    {stringify(seeRookValue), &seeRookValue},
    {stringify(seeQueenValue), &seeQueenValue}
};
