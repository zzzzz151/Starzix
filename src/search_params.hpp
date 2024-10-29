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

    MAYBE_CONSTEXPR TunableParam(const T _value, const T _min, const T _max, const T _step)
    {
        value = _value; min = _min; max = _max; step = _step;

        assert(max > min);
        assert(value >= min && value <= max);
        assert(step > 0);
    }

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

constexpr i32 MAX_DEPTH = 100;

// Time management
MAYBE_CONSTEXPR TunableParam<double> hardTimePercentage = TunableParam<double>(0.73, 0.5, 0.75, 0.25 / 4.0);
MAYBE_CONSTEXPR TunableParam<double> softTimePercentage = TunableParam<double>(0.105, 0.05, 0.25, 0.02);

// Eval scale with material / game phase
MAYBE_CONSTEXPR TunableParam<float> evalMaterialScaleMin = TunableParam<float>(0.75, 0.75, 1.0, 0.25 / 4.0);
MAYBE_CONSTEXPR TunableParam<float> evalMaterialScaleMax = TunableParam<float>(1.06, 1.0, 1.25, 0.25 / 4.0);
MAYBE_CONSTEXPR i32 MATERIAL_MAX = 62;

// SEE
MAYBE_CONSTEXPR TunableParam<i32> seePawnValue  = TunableParam<i32>(160, 50, 200, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeMinorValue = TunableParam<i32>(350, 200, 400, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeRookValue  = TunableParam<i32>(602, 300, 700, 50);
MAYBE_CONSTEXPR TunableParam<i32> seeQueenValue = TunableParam<i32>(1106, 600, 1200, 100);

MAYBE_CONSTEXPR std::array<i32, 7> SEE_PIECE_VALUES = {
    seePawnValue(), seeMinorValue(), seeMinorValue(), seeRookValue(),  seeQueenValue(), 0, 0
};

// Aspiration windows
MAYBE_CONSTEXPR TunableParam<i32>    aspMinDepth     = TunableParam<i32>(7, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32>    aspInitialDelta = TunableParam<i32>(13, 5, 25, 5);
MAYBE_CONSTEXPR TunableParam<double> aspDeltaMul     = TunableParam<double>(1.37, 1.25, 2.0, 0.15);

// Improving flag
MAYBE_CONSTEXPR TunableParam<i32> improvingThreshold = TunableParam<i32>(8, 1, 51, 5);

// RFP (Reverse futility pruning)
MAYBE_CONSTEXPR TunableParam<i32> rfpMaxDepth = TunableParam<i32>(7, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> rfpDepthMul = TunableParam<i32>(71, 40, 130, 10);

// Razoring
MAYBE_CONSTEXPR TunableParam<i32> razoringMaxDepth = TunableParam<i32>(6, 2, 6, 2);
MAYBE_CONSTEXPR TunableParam<i32> razoringDepthMul = TunableParam<i32>(407, 250, 450, 25);

// NMP (Null move pruning)
MAYBE_CONSTEXPR TunableParam<i32>   nmpMinDepth       = TunableParam<i32>(3, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<i32>   nmpBaseReduction  = TunableParam<i32>(4, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<float> nmpDepthMul       = TunableParam<float>(0.4, 0.25, 0.55, 0.1);
MAYBE_CONSTEXPR TunableParam<i32>   nmpEvalBetaMargin = TunableParam<i32>(100, 40, 250, 30);
MAYBE_CONSTEXPR TunableParam<i32>   nmpEvalBetaMul    = TunableParam<i32>(12, 4, 40, 4);

// Probcut
MAYBE_CONSTEXPR TunableParam<i32>   probcutMargin              = TunableParam<i32>(237, 100, 340, 30);
MAYBE_CONSTEXPR TunableParam<float> probcutImprovingPercentage = TunableParam<float>(0.3, 0.25, 0.75, 0.1);

// IIR (Internal iterative reduction)
MAYBE_CONSTEXPR TunableParam<i32> iirMinDepth = TunableParam<i32>(4, 4, 6, 1);

// LMP (Late move pruning)
MAYBE_CONSTEXPR TunableParam<i32>   lmpMinMoves = TunableParam<i32>(2, 2, 4, 1);
MAYBE_CONSTEXPR TunableParam<float> lmpDepthMul = TunableParam<float>(1.15, 0.75, 1.25, 0.1);

// FP (Futility pruning)
MAYBE_CONSTEXPR TunableParam<i32> fpMaxDepth = TunableParam<i32>(7, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> fpBase     = TunableParam<i32>(160, 40, 260, 20);
MAYBE_CONSTEXPR TunableParam<i32> fpDepthMul = TunableParam<i32>(165, 40, 260, 20);

// SEE threshold and pruning
MAYBE_CONSTEXPR TunableParam<i32> seePruningMaxDepth = TunableParam<i32>(9, 7, 11, 1);
MAYBE_CONSTEXPR TunableParam<i32> seeNoisyThreshold  = TunableParam<i32>(-103, -150, -10, 20);
MAYBE_CONSTEXPR TunableParam<i32> seeQuietThreshold  = TunableParam<i32>(-77, -150, -10, 20);
MAYBE_CONSTEXPR TunableParam<float> seeNoisyHistMul  = TunableParam<float>(0.0475, 0.0, 0.125, 0.025);
MAYBE_CONSTEXPR TunableParam<float> seeQuietHistMul  = TunableParam<float>(0.03, 0.0, 0.12, 0.02);

// SE (Singular extensions)
MAYBE_CONSTEXPR TunableParam<i32> singularMinDepth      = TunableParam<i32>(6, 6, 10, 1);
MAYBE_CONSTEXPR TunableParam<i32> singularDepthMargin   = TunableParam<i32>(4, 1, 5, 1);
MAYBE_CONSTEXPR TunableParam<i32> doubleExtensionMargin = TunableParam<i32>(36, 2, 42, 10);
MAYBE_CONSTEXPR u8 DOUBLE_EXTENSIONS_MAX = 10;

// LMR (Late move reductions)
MAYBE_CONSTEXPR TunableParam<double> lmrBaseQuiet       = TunableParam<double>(0.62, 0.4, 1.0, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrBaseNoisy       = TunableParam<double>(0.6, 0.4, 1.0, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrMultiplierQuiet = TunableParam<double>(0.51, 0.3, 0.8, 0.1);
MAYBE_CONSTEXPR TunableParam<double> lmrMultiplierNoisy = TunableParam<double>(0.3, 0.3, 0.8, 0.1);
MAYBE_CONSTEXPR TunableParam<i32>    lmrQuietHistoryDiv = TunableParam<i32>(28073, 8192, 32768, 2048);
MAYBE_CONSTEXPR TunableParam<i32>    lmrNoisyHistoryDiv = TunableParam<i32>(3430, 1024, 16384, 1024);

// Deeper search in PVS with LMR
MAYBE_CONSTEXPR TunableParam<i32> deeperBase = TunableParam<i32>(54, 15, 95, 20);

// History max
MAYBE_CONSTEXPR i32 HISTORY_MAX = 16384;

// History bonus
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMul    = TunableParam<i32>(313, 50, 600, 25);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusOffset = TunableParam<i32>(18, 0, 500, 100);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMax    = TunableParam<i32>(1922, 500, 2500, 200);

// History malus
MAYBE_CONSTEXPR TunableParam<i32> historyMalusMul    = TunableParam<i32>(272, 50, 600, 25);
MAYBE_CONSTEXPR TunableParam<i32> historyMalusOffset = TunableParam<i32>(30, 0, 500, 100);
MAYBE_CONSTEXPR TunableParam<i32> historyMalusMax    = TunableParam<i32>(1142, 500, 2500, 200);

// History weights
MAYBE_CONSTEXPR TunableParam<float> mainHistoryWeight  = TunableParam<float>(0.85, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist1PlyWeight = TunableParam<float>(1.06, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist2PlyWeight = TunableParam<float>(1.16, 0.2, 4.0, 0.2);
MAYBE_CONSTEXPR TunableParam<float> contHist4PlyWeight = TunableParam<float>(0.64, 0.2, 4.0, 0.2);

MAYBE_CONSTEXPR std::array<float, 3> CONT_HIST_WEIGHTS = {
    contHist1PlyWeight(), contHist2PlyWeight(), contHist4PlyWeight()
};

// Correction histories
constexpr u64 CORR_HIST_SIZE = 16384;
MAYBE_CONSTEXPR TunableParam<float> corrHistPawnsWeight    = TunableParam<float>(0.0063, 0.002, 0.02, 0.002);
MAYBE_CONSTEXPR TunableParam<float> corrHistNonPawnsWeight = TunableParam<float>(0.0076, 0.002, 0.02, 0.002);
MAYBE_CONSTEXPR TunableParam<float> corrHistLastMoveWeight = TunableParam<float>(0.0043, 0.002, 0.02, 0.002);

MAYBE_CONSTEXPR std::array<float, 4> CORR_HISTS_WEIGHTS {
    corrHistPawnsWeight(), corrHistNonPawnsWeight(), corrHistNonPawnsWeight(), corrHistLastMoveWeight()
};

// [isQuietMove][depth][moveIndex]
inline MultiArray<i32, 2, MAX_DEPTH + 1, 256> getLmrTable()
{
    MultiArray<i32, 2, MAX_DEPTH + 1, 256> lmrTable = { };

    for (int depth = 1; depth < MAX_DEPTH + 1; depth++)
        for (int move = 1; move < 256; move++)
        {
            lmrTable[false][depth][move]
                = round(lmrBaseNoisy() + ln(depth) * ln(move) * lmrMultiplierNoisy());

            lmrTable[true][depth][move]
                = round(lmrBaseQuiet() + ln(depth) * ln(move) * lmrMultiplierQuiet());
        }

    return lmrTable;
};

// [isQuietMove][depth][moveIndex]
MAYBE_CONST MultiArray<i32, 2, MAX_DEPTH+1, 256> LMR_TABLE = getLmrTable();

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
        {stringify(aspMinDepth), &aspMinDepth},
        {stringify(aspInitialDelta), &aspInitialDelta},
        {stringify(aspDeltaMul), &aspDeltaMul},
        {stringify(improvingThreshold), &improvingThreshold},
        {stringify(rfpMaxDepth), &rfpMaxDepth},
        {stringify(rfpDepthMul), &rfpDepthMul},
        {stringify(razoringMaxDepth), &razoringMaxDepth},
        {stringify(razoringDepthMul), &razoringDepthMul},
        {stringify(nmpMinDepth), &nmpMinDepth},
        {stringify(nmpBaseReduction), &nmpBaseReduction},
        {stringify(nmpDepthMul), &nmpDepthMul},
        {stringify(nmpEvalBetaMargin), &nmpEvalBetaMargin},
        {stringify(nmpEvalBetaMul), &nmpEvalBetaMul},
        {stringify(probcutMargin), &probcutMargin},
        {stringify(probcutImprovingPercentage), &probcutImprovingPercentage},
        {stringify(iirMinDepth), &iirMinDepth},
        {stringify(lmpMinMoves), &lmpMinMoves},
        {stringify(lmpDepthMul), &lmpDepthMul},
        {stringify(fpMaxDepth), &fpMaxDepth},
        {stringify(fpBase), &fpBase},
        {stringify(fpDepthMul), &fpDepthMul},
        {stringify(seePruningMaxDepth), &seePruningMaxDepth},
        {stringify(seeNoisyThreshold), &seeNoisyThreshold},
        {stringify(seeQuietThreshold), &seeQuietThreshold},
        {stringify(seeNoisyHistMul), &seeNoisyHistMul},
        {stringify(seeQuietHistMul), &seeQuietHistMul},
        {stringify(singularMinDepth), &singularMinDepth},
        {stringify(singularDepthMargin), &singularDepthMargin},
        {stringify(doubleExtensionMargin), &doubleExtensionMargin},
        {stringify(lmrBaseQuiet), &lmrBaseQuiet},
        {stringify(lmrBaseNoisy), &lmrBaseNoisy},
        {stringify(lmrMultiplierQuiet), &lmrMultiplierQuiet},
        {stringify(lmrMultiplierNoisy), &lmrMultiplierNoisy},
        {stringify(lmrQuietHistoryDiv), &lmrQuietHistoryDiv},
        {stringify(lmrNoisyHistoryDiv), &lmrNoisyHistoryDiv},
        {stringify(deeperBase), &deeperBase},
        {stringify(historyBonusMul), &historyBonusMul},
        {stringify(historyBonusOffset), &historyBonusOffset},
        {stringify(historyBonusMax), &historyBonusMax},
        {stringify(historyMalusMul), &historyMalusMul},
        {stringify(historyMalusOffset), &historyMalusOffset},
        {stringify(historyMalusMax), &historyMalusMax},
        {stringify(mainHistoryWeight), &mainHistoryWeight},
        {stringify(contHist1PlyWeight), &contHist1PlyWeight},
        {stringify(contHist2PlyWeight), &contHist2PlyWeight},
        {stringify(contHist4PlyWeight), &contHist4PlyWeight},
        {stringify(corrHistPawnsWeight), &corrHistPawnsWeight},
        {stringify(corrHistNonPawnsWeight), &corrHistNonPawnsWeight},
        {stringify(corrHistLastMoveWeight), &corrHistLastMoveWeight},
    };
#endif
