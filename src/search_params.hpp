// clang-format off

#pragma once

#include "utils.hpp"

#if defined(TUNE)
    #include "3rdparty/ordered_map.h"
    #include <variant>

    #define MAYBE_CONSTEXPR
    #define MAYBE_CONST
#else
    #define MAYBE_CONSTEXPR constexpr
    #define MAYBE_CONST const
#endif

template <typename T> struct TunableParam {
    public:
    T value, min, max, step;

    constexpr TunableParam(const T value, const T min, const T max, const T step) 
    : value(value), min(min), max(max), step(step) { }

    // overload function operator
    constexpr T operator () () const {
        return value;
    }

    constexpr bool floatOrDouble() const {
        return std::is_same<decltype(value), double>::value
            || std::is_same<decltype(value), float>::value;
    }

};

constexpr i32 INF = 32000;
constexpr i32 MIN_MATE_SCORE = INF - 256;
constexpr i32 VALUE_NONE = INF + 1;

constexpr u8 MAX_DEPTH = 100;

// Time management
MAYBE_CONSTEXPR TunableParam<double> hardTimePercentage = TunableParam<double>(0.7, 0.25, 0.75, 0.1);
MAYBE_CONSTEXPR TunableParam<double> softTimePercentage = TunableParam<double>(0.11, 0.01, 0.15, 0.02);

// Eval scale with material / game phase
MAYBE_CONSTEXPR TunableParam<float> evalMaterialScaleMin = TunableParam<float>(0.7, 0.5, 1.0, 0.1);
MAYBE_CONSTEXPR TunableParam<float> evalMaterialScaleMax = TunableParam<float>(1.02, 1.0, 1.5, 0.1);
MAYBE_CONSTEXPR i32 MATERIAL_MAX = 62;

// SEE
MAYBE_CONSTEXPR TunableParam<i32> seePawnValue    = TunableParam<i32>(156, 1, 201, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeMinorValue   = TunableParam<i32>(346, 150, 450, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeRookValue    = TunableParam<i32>(519, 300, 700, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeQueenValue   = TunableParam<i32>(1079 , 600, 1200, 100);
MAYBE_CONSTEXPR TunableParam<i32> seeNoisyHistDiv = TunableParam<i32>(16, 8, 32, 2);
MAYBE_CONSTEXPR TunableParam<i32> seeQuietHistDiv = TunableParam<i32>(64, 32, 256, 16); 

MAYBE_CONSTEXPR std::array<i32, 7> SEE_PIECE_VALUES = {
    seePawnValue(),  // Pawn
    seeMinorValue(), // Knight
    seeMinorValue(), // Bishop
    seeRookValue(),  // Rook
    seeQueenValue(), // Queen
    0,               // King
    0                // None
};

// Aspiration windows
MAYBE_CONSTEXPR TunableParam<i32>    aspMinDepth        = TunableParam<i32>(6, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32>    aspInitialDelta    = TunableParam<i32>(13, 5, 25, 5);
MAYBE_CONSTEXPR TunableParam<double> aspDeltaMultiplier = TunableParam<double>(1.37, 1.2, 2.0, 0.1);

// Improving flag
MAYBE_CONSTEXPR TunableParam<i32> improvingThreshold = TunableParam<i32>(9, 1, 51, 5);

// RFP (Reverse futility pruning)
MAYBE_CONSTEXPR TunableParam<i32> rfpMaxDepth   = TunableParam<i32>(7, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> rfpMultiplier = TunableParam<i32>(65, 40, 130, 10);

// Razoring
MAYBE_CONSTEXPR TunableParam<i32> razoringMaxDepth   = TunableParam<i32>(5, 2, 6, 2);
MAYBE_CONSTEXPR TunableParam<i32> razoringMultiplier = TunableParam<i32>(388, 250, 450, 50);

// NMP (Null move pruning)
MAYBE_CONSTEXPR TunableParam<i32>   nmpMinDepth           = TunableParam<i32>(3, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<i32>   nmpBaseReduction      = TunableParam<i32>(4, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<float> nmpReductionDivisor   = TunableParam<float>(2.7, 2.0, 4.0, 0.5);
MAYBE_CONSTEXPR TunableParam<i32>   nmpEvalBetaMargin     = TunableParam<i32>(90, 20, 260, 20);
MAYBE_CONSTEXPR TunableParam<i32>   nmpEvalBetaMultiplier = TunableParam<i32>(13, 4, 40, 4);

// Probcut
MAYBE_CONSTEXPR TunableParam<i32>   probcutMargin              = TunableParam<i32>(229, 100, 350, 50);
MAYBE_CONSTEXPR TunableParam<float> probcutImprovingPercentage = TunableParam<float>(0.28, 0.25, 0.75, 0.1);

// IIR (Internal iterative reduction)
MAYBE_CONSTEXPR TunableParam<i32> iirMinDepth = TunableParam<i32>(4, 4, 6, 1);

// LMP (Late move pruning)
MAYBE_CONSTEXPR TunableParam<i32>   lmpMinMoves   = TunableParam<i32>(2, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<float> lmpMultiplier = TunableParam<float>(1.08, 0.5, 1.5, 0.1);

// FP (Futility pruning)
MAYBE_CONSTEXPR TunableParam<i32> fpMaxDepth   = TunableParam<i32>(7, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> fpBase       = TunableParam<i32>(160, 40, 260, 20);
MAYBE_CONSTEXPR TunableParam<i32> fpMultiplier = TunableParam<i32>(157, 40, 260, 20);

// SEE pruning
MAYBE_CONSTEXPR TunableParam<i32> seePruningMaxDepth = TunableParam<i32>(8, 7, 11, 1);
MAYBE_CONSTEXPR TunableParam<i32> seeQuietThreshold  = TunableParam<i32>(-55, -161, -1, 20);
MAYBE_CONSTEXPR TunableParam<i32> seeNoisyThreshold  = TunableParam<i32>(-124, -161, -1, 20);

// SE (Singular extensions)
MAYBE_CONSTEXPR TunableParam<i32> singularMinDepth      = TunableParam<i32>(6, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> singularDepthMargin   = TunableParam<i32>(4, 1, 5, 1);
MAYBE_CONSTEXPR TunableParam<i32> doubleExtensionMargin = TunableParam<i32>(40, 2, 42, 10);
MAYBE_CONSTEXPR u8 DOUBLE_EXTENSIONS_MAX = 10;

// LMR (Late move reductions)
MAYBE_CONSTEXPR TunableParam<double> lmrBaseQuiet       = TunableParam<double>(0.735, 0.4, 1.2, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrMultiplierQuiet = TunableParam<double>(0.5, 0.2, 0.8, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrBaseNoisy       = TunableParam<double>(0.685, 0.4, 1.2, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrMultiplierNoisy = TunableParam<double>(0.33, 0.2, 0.8, 0.1);
MAYBE_CONSTEXPR TunableParam<i32>    lmrQuietHistoryDiv = TunableParam<i32>(29757, 8192, 32768, 2048);
MAYBE_CONSTEXPR TunableParam<i32>    lmrNoisyHistoryDiv = TunableParam<i32>(3499, 1024, 16384, 1024);

// Deeper search in PVS with LMR
MAYBE_CONSTEXPR TunableParam<i32> deeperBase = TunableParam<i32>(41, 15, 90, 15);

// History max
MAYBE_CONSTEXPR i32 HISTORY_MAX = 16384;

// History bonus
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMultiplier = TunableParam<i32>(300, 50, 600, 50);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusOffset     = TunableParam<i32>(38, 0, 500, 100);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMax        = TunableParam<i32>(1730, 500, 2500, 200);

// History malus
MAYBE_CONSTEXPR TunableParam<i32> historyMalusMultiplier = TunableParam<i32>(272, 50, 600, 50);
MAYBE_CONSTEXPR TunableParam<i32> historyMalusOffset     = TunableParam<i32>(12, 0, 500, 100);
MAYBE_CONSTEXPR TunableParam<i32> historyMalusMax        = TunableParam<i32>(1381, 500, 2500, 200);

// History weights
MAYBE_CONSTEXPR TunableParam<float> mainHistoryWeight  = TunableParam<float>(1.066, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist1PlyWeight = TunableParam<float>(0.88, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist2PlyWeight = TunableParam<float>(1.136, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist4PlyWeight = TunableParam<float>(0.47, 0.2, 4.0, 0.2);

MAYBE_CONSTEXPR std::array<float, 3> CONT_HISTORY_WEIGHTS = { 
    contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight() 
};

// Correction history
MAYBE_CONSTEXPR TunableParam<i32> corrHistDiv = TunableParam<i32>(165, 32, 512, 32);

#if defined(TUNE)
    using TunableParamVariant = std::variant<
        TunableParam<i32>*, 
        TunableParam<float>*,
        TunableParam<double>*
    >;

    tsl::ordered_map<std::string, TunableParamVariant> tunableParams = {
        {stringify(hardTimePercentage), &hardTimePercentage},
        {stringify(softTimePercentage), &softTimePercentage},
        {stringify(evalMaterialScaleMin), &evalMaterialScaleMin},
        {stringify(evalMaterialScaleMax), &evalMaterialScaleMax},
        {stringify(seePawnValue), &seePawnValue},
        {stringify(seeMinorValue), &seeMinorValue},
        {stringify(seeRookValue), &seeRookValue},
        {stringify(seeQueenValue), &seeQueenValue},
        {stringify(seeNoisyHistDiv), &seeNoisyHistDiv},
        {stringify(seeQuietHistDiv), &seeQuietHistDiv},
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
        {stringify(nmpEvalBetaMargin), &nmpEvalBetaMargin},
        {stringify(nmpEvalBetaMultiplier), &nmpEvalBetaMultiplier},
        {stringify(probcutMargin), &probcutMargin},
        {stringify(probcutImprovingPercentage), &probcutImprovingPercentage},
        {stringify(iirMinDepth), &iirMinDepth},
        {stringify(lmpMinMoves), &lmpMinMoves},
        {stringify(lmpMultiplier), &lmpMultiplier},
        {stringify(fpMaxDepth), &fpMaxDepth},
        {stringify(fpBase), &fpBase},
        {stringify(fpMultiplier), &fpMultiplier},
        {stringify(seePruningMaxDepth), &seePruningMaxDepth},
        {stringify(seeQuietThreshold), &seeQuietThreshold},
        {stringify(seeNoisyThreshold), &seeNoisyThreshold},
        {stringify(singularMinDepth), &singularMinDepth},
        {stringify(singularDepthMargin), &singularDepthMargin},
        {stringify(doubleExtensionMargin), &doubleExtensionMargin},
        {stringify(lmrBaseQuiet), &lmrBaseQuiet},
        {stringify(lmrMultiplierQuiet), &lmrMultiplierQuiet},
        {stringify(lmrBaseNoisy), &lmrBaseNoisy},
        {stringify(lmrMultiplierNoisy), &lmrMultiplierNoisy},
        {stringify(lmrQuietHistoryDiv), &lmrQuietHistoryDiv},
        {stringify(lmrNoisyHistoryDiv), &lmrNoisyHistoryDiv},
        {stringify(deeperBase), &deeperBase},
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
        {stringify(corrHistDiv), &corrHistDiv}
    };
#endif
