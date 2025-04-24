// clang-format off

#pragma once

#include "utils.hpp"

#if defined(TUNE)
    #include "../3rd-party/ordered_map.h"
    #include <variant>

    #define MAYBE_CONSTEXPR
    #define MAYBE_CONST
    #define CONSTEXPR_OR_CONST const
#else
    #define MAYBE_CONSTEXPR constexpr
    #define MAYBE_CONST const
    #define CONSTEXPR_OR_CONST constexpr
#endif

constexpr size_t MAX_DEPTH = 100;
constexpr i32 INF = 30'000;
constexpr i32 MIN_MATE_SCORE = INF - static_cast<i32>(MAX_DEPTH);

template <typename T>
struct TunableParam
{
public:

    T value, min, max, step;

    constexpr TunableParam(const T _value, const T _min, const T _max, const T _step)
    {
        value = _value; min = _min; max = _max; step = _step;

        assert(max > min);
        assert(value >= min && value <= max);
        assert(step > 0);
    }

    // Function operator
    constexpr T operator () () const
    {
        return value;
    }

    constexpr TunableParam operator/(const T x)
    {
        return TunableParam(value / x, min / x, max / x, step / x);
    }

    constexpr bool isFloatOrDouble() const
    {
        return std::is_same<decltype(value), float>::value
            || std::is_same<decltype(value), double>::value;
    }

}; // struct TunableParam

// Base time management
MAYBE_CONSTEXPR auto tmHardPercentage = TunableParam<double>(0.71, 0.25, 0.75, 0.1);
MAYBE_CONSTEXPR auto tmSoftPercentage = TunableParam<double>(12.5, 2.0, 20.0, 2.0) / 100.0;

// Nodes TM
MAYBE_CONSTEXPR auto tmNodesBase = TunableParam<double>(1.23, 1.0, 2.0, 0.1);
MAYBE_CONSTEXPR auto tmNodesMul  = TunableParam<double>(1.0, 0.5, 1.0, 0.1);

// Score stability TM
MAYBE_CONSTEXPR auto tmScoreStabThreshold = TunableParam<i32>(10, 0, 50, 10);
MAYBE_CONSTEXPR auto tmScoreStabBase      = TunableParam<double>(1.5, 1.0, 2.0, 0.1);
MAYBE_CONSTEXPR auto tmScoreStabMul       = TunableParam<double>(0.1, 0.0, 0.5, 0.05);
MAYBE_CONSTEXPR auto tmScoreStabMin       = TunableParam<double>(0.5, 0.1, 1.0, 0.1);

// Best move stability TM
MAYBE_CONSTEXPR auto tmBestMovStabBase = TunableParam<double>(1.5, 1.0, 2.0, 0.1);
MAYBE_CONSTEXPR auto tmBestMovStabMul  = TunableParam<double>(0.1, 0.0, 0.5, 0.05);
MAYBE_CONSTEXPR auto tmBestMovStabMin  = TunableParam<double>(0.5, 0.1, 1.0, 0.1);

// Pieces values
MAYBE_CONSTEXPR auto pawnValue  = TunableParam<i32>(170, 50, 200, 50);
MAYBE_CONSTEXPR auto minorValue = TunableParam<i32>(333, 200, 500, 50);
MAYBE_CONSTEXPR auto rookValue  = TunableParam<i32>(512, 500, 900, 50);
MAYBE_CONSTEXPR auto queenValue = TunableParam<i32>(984, 900, 1500, 100);

// Aspiration windows
MAYBE_CONSTEXPR auto aspStartDelta = TunableParam<i32>(16, 5, 25, 5);
MAYBE_CONSTEXPR auto aspDeltaMul   = TunableParam<double>(1.37, 1.2, 2.0, 0.1);

// RFP (Reverse futility pruning)
MAYBE_CONSTEXPR auto rfpDepthMul = TunableParam<i32>(72, 30, 150, 10);

// Razoring
MAYBE_CONSTEXPR auto razoringBase     = TunableParam<i32>(365, 150, 600, 50);
MAYBE_CONSTEXPR auto razoringDepthMul = TunableParam<i32>(276, 150, 600, 15);

// FP (Futility pruning)
MAYBE_CONSTEXPR auto fpBase         = TunableParam<i32>(130, 40, 260, 20);
MAYBE_CONSTEXPR auto fpDepthMul     = TunableParam<i32>(175, 40, 260, 20);
MAYBE_CONSTEXPR auto fpQuietHistMul = TunableParam<float>(2.75f, 0.0f, 4.0f, 0.5f) / 100.0f;
MAYBE_CONSTEXPR auto fpQsMargin     = TunableParam<i32>(140, 40, 260, 20);

// SEE thresholds
MAYBE_CONSTEXPR auto seeNoisyThreshold = TunableParam<i32>(-198, -210, -10, 20);
MAYBE_CONSTEXPR auto seeQuietThreshold = TunableParam<i32>(-82, -210, -10, 20);
MAYBE_CONSTEXPR auto seeNoisyHistMul   = TunableParam<float>(3.76f, 0.0f, 6.0f, 0.5f)  / 100.0f;
MAYBE_CONSTEXPR auto seeQuietHistMul   = TunableParam<float>(1.53f, 0.0f, 2.0f, 0.25f) / 100.0f;

// SE (Singular extensions)
MAYBE_CONSTEXPR auto doubleExtMargin = TunableParam<i32>(29, 1, 51, 10);

// LMR (Late move reductions)
MAYBE_CONSTEXPR auto lmrBaseNoisy    = TunableParam<double>(0.65, 0.3, 1.2, 0.1);
MAYBE_CONSTEXPR auto lmrBaseQuiet    = TunableParam<double>(0.87, 0.3, 1.2, 0.1);
MAYBE_CONSTEXPR auto lmrMulNoisy     = TunableParam<double>(0.21, 0.2, 0.8, 0.1);
MAYBE_CONSTEXPR auto lmrMulQuiet     = TunableParam<double>(0.48, 0.2, 0.8, 0.1);
MAYBE_CONSTEXPR auto lmrQuietHistMul = TunableParam<float>(8.76f, 0.0f, 9.0f, 1.0f) / 100'000.0f;

// PVS + LMR
MAYBE_CONSTEXPR auto deeperBase      = TunableParam<i32>(67, 0, 100, 20);
MAYBE_CONSTEXPR auto shallowerMargin = TunableParam<i32>(35, 0, 100, 20);

// History heuristic
constexpr i32 HISTORY_MAX = 16384;
MAYBE_CONSTEXPR auto histBonusMul    = TunableParam<i32>(306, 50, 600, 25);
MAYBE_CONSTEXPR auto histBonusOffset = TunableParam<i32>(19, 0, 500, 100);
MAYBE_CONSTEXPR auto histBonusMax    = TunableParam<i32>(1824, 500, 2500, 200);
MAYBE_CONSTEXPR auto histMalusMul    = TunableParam<i32>(357, 50, 600, 25);
MAYBE_CONSTEXPR auto histMalusOffset = TunableParam<i32>(57, 0, 500, 100);
MAYBE_CONSTEXPR auto histMalusMax    = TunableParam<i32>(1166, 500, 2500, 200);

// Correction histories
constexpr size_t CORR_HIST_SIZE = 16384;
MAYBE_CONSTEXPR auto corrHistPawnsWeight    = TunableParam<float>(0.71f, .0f, 2.0f, .2f) / 100.0f;
MAYBE_CONSTEXPR auto corrHistNonPawnsWeight = TunableParam<float>(1.19f, .0f, 2.0f, .2f) / 100.0f;
MAYBE_CONSTEXPR auto corrHistLastMoveWeight = TunableParam<float>(0.49f, .0f, 2.0f, .2f) / 100.0f;
MAYBE_CONSTEXPR auto corrHistContWeight     = TunableParam<float>(0.73f, .0f, 2.0f, .2f) / 100.0f;

// [depth][isQuietMove][legalMovesSeen]
inline MultiArray<i32, MAX_DEPTH + 1, 2, 256> getLmrTable()
{
    MultiArray<i32, MAX_DEPTH + 1, 2, 256> lmrTable = { };

    for (size_t depth = 1; depth < MAX_DEPTH + 1; depth++)
        for (size_t legalMovesSeen = 1; legalMovesSeen < 256; legalMovesSeen++)
        {
            const double a = ln(static_cast<double>(depth));
            const double b = ln(static_cast<double>(legalMovesSeen));

            lmrTable[depth][false][legalMovesSeen] = static_cast<i32>(lround(
                lmrBaseNoisy() + a * b * lmrMulNoisy()
            ));

            lmrTable[depth][true][legalMovesSeen] = static_cast<i32>(lround(
                lmrBaseQuiet() + a * b * lmrMulQuiet()
            ));
        }

    return lmrTable;
}

MAYBE_CONST auto LMR_TABLE = getLmrTable();

#if defined(TUNE)
    using TunableParamVariant = std::variant<
        TunableParam<i32>*,
        TunableParam<float>*,
        TunableParam<double>*
    >;

    tsl::ordered_map<std::string, TunableParamVariant> tunableParams =
    {
        { stringify(tmHardPercentage),       &tmHardPercentage },
        { stringify(tmSoftPercentage),       &tmSoftPercentage },
        { stringify(tmNodesBase),            &tmNodesBase },
        { stringify(tmNodesMul),             &tmNodesMul },
        { stringify(tmScoreStabThreshold),   &tmScoreStabThreshold },
        { stringify(tmScoreStabBase),        &tmScoreStabBase },
        { stringify(tmScoreStabMul),         &tmScoreStabMul },
        { stringify(tmScoreStabMin),         &tmScoreStabMin },
        { stringify(tmBestMovStabBase),      &tmBestMovStabBase },
        { stringify(tmBestMovStabMul),       &tmBestMovStabMul },
        { stringify(tmBestMovStabMin),       &tmBestMovStabMin },
        { stringify(pawnValue),              &pawnValue },
        { stringify(minorValue),             &minorValue },
        { stringify(rookValue),              &rookValue },
        { stringify(queenValue),             &queenValue },
        { stringify(aspStartDelta),          &aspStartDelta },
        { stringify(aspDeltaMul),            &aspDeltaMul },
        { stringify(rfpDepthMul),            &rfpDepthMul },
        { stringify(razoringBase),           &razoringBase },
        { stringify(razoringDepthMul),       &razoringDepthMul },
        { stringify(fpBase),                 &fpBase },
        { stringify(fpDepthMul),             &fpDepthMul },
        { stringify(fpQuietHistMul),         &fpQuietHistMul },
        { stringify(fpQsMargin),             &fpQsMargin },
        { stringify(seeNoisyThreshold),      &seeNoisyThreshold },
        { stringify(seeQuietThreshold),      &seeQuietThreshold },
        { stringify(seeNoisyHistMul),        &seeNoisyHistMul },
        { stringify(seeQuietHistMul),        &seeQuietHistMul },
        { stringify(doubleExtMargin),        &doubleExtMargin },
        { stringify(lmrBaseNoisy),           &lmrBaseNoisy },
        { stringify(lmrBaseQuiet),           &lmrBaseQuiet },
        { stringify(lmrMulNoisy),            &lmrMulNoisy },
        { stringify(lmrMulQuiet),            &lmrMulQuiet },
        { stringify(lmrQuietHistMul),        &lmrQuietHistMul },
        { stringify(deeperBase),             &deeperBase },
        { stringify(shallowerMargin),        &shallowerMargin },
        { stringify(histBonusMul),           &histBonusMul },
        { stringify(histBonusOffset),        &histBonusOffset },
        { stringify(histBonusMax),           &histBonusMax },
        { stringify(histMalusMul),           &histMalusMul },
        { stringify(histMalusOffset),        &histMalusOffset },
        { stringify(histMalusMax),           &histMalusMax },
        { stringify(corrHistPawnsWeight),    &corrHistPawnsWeight },
        { stringify(corrHistNonPawnsWeight), &corrHistNonPawnsWeight },
        { stringify(corrHistLastMoveWeight), &corrHistLastMoveWeight },
        { stringify(corrHistContWeight),     &corrHistContWeight },
    };

    inline void printSpsaInput()
    {
        for (const auto& pair : tunableParams)
        {
            const std::string paramName = pair.first;
            const auto& tunableParam = pair.second;

            std::visit([&paramName] (auto* myParam)
            {
                if (myParam == nullptr) return;

                std::cout << paramName
                          << ", " << (myParam->isFloatOrDouble() ? "float" : "int")
                          << ", " << std::fixed << myParam->value
                          << ", " << std::fixed << myParam->min
                          << ", " << std::fixed << myParam->max
                          << ", " << std::fixed << myParam->step
                          << ", 0.002"
                          << "\n";
            }, tunableParam);

            std::flush(std::cout);
        }
    }
#endif
