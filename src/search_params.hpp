// clang-format off

#pragma once

#include "utils.hpp"

#if defined(TUNE)
    #include "../3rd-party/ordered_map.h"
    #include <variant>

    #define MAYBE_CONSTEXPR
    #define MAYBE_CONST
#else
    #define MAYBE_CONSTEXPR constexpr
    #define MAYBE_CONST const
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

    // overload function operator
    constexpr T operator () () const
    {
        return value;
    }

    constexpr bool isFloatOrDouble() const
    {
        return std::is_same<decltype(value), float>::value
            || std::is_same<decltype(value), double>::value;
    }

}; // struct TunableParam

// Time management
MAYBE_CONSTEXPR auto timeHardPercentage = TunableParam<double>(0.5, 0.25, 0.75, 0.1);
MAYBE_CONSTEXPR auto timeSoftPercentage = TunableParam<double>(0.05, 0.02, 0.20, 0.02);
MAYBE_CONSTEXPR auto nodesTmBase = TunableParam<double>(1.5, 1.0, 2.0, 0.1);
MAYBE_CONSTEXPR auto nodesTmMul  = TunableParam<double>(0.75, 0.5, 1.0, 0.1);

// RFP (Reverse futility pruning)
MAYBE_CONSTEXPR auto rfpDepthMul = TunableParam<i32>(75, 30, 180, 10);

// SEE thresholds
MAYBE_CONSTEXPR auto seeNoisyHistMul   = TunableParam<float>(0.05f, 0.0f, 0.2f, 0.02f);
MAYBE_CONSTEXPR auto seeNoisyThreshold = TunableParam<i32>(-150, -210, -10, 20);
MAYBE_CONSTEXPR auto seeQuietThreshold = TunableParam<i32>(-90, -210, -10, 20);

// LMR (Late move reductions)
MAYBE_CONSTEXPR auto lmrBaseNoisy = TunableParam<double>(0.8, 0.3, 1.2, 0.1);
MAYBE_CONSTEXPR auto lmrBaseQuiet = TunableParam<double>(0.8, 0.3, 1.2, 0.1);
MAYBE_CONSTEXPR auto lmrMulNoisy  = TunableParam<double>(0.4, 0.2, 0.8, 0.1);
MAYBE_CONSTEXPR auto lmrMulQuiet  = TunableParam<double>(0.4, 0.2, 0.8, 0.1);

// History heuristic
constexpr i32 HISTORY_MAX = 16384;
MAYBE_CONSTEXPR auto historyBonusMul    = TunableParam<i32>(300, 50, 600, 25);
MAYBE_CONSTEXPR auto historyBonusOffset = TunableParam<i32>(0, 0, 500, 100);
MAYBE_CONSTEXPR auto historyBonusMax    = TunableParam<i32>(1500, 500, 2500, 200);
MAYBE_CONSTEXPR auto historyMalusMul    = TunableParam<i32>(300, 50, 600, 25);
MAYBE_CONSTEXPR auto historyMalusOffset = TunableParam<i32>(0, 0, 500, 100);
MAYBE_CONSTEXPR auto historyMalusMax    = TunableParam<i32>(1500, 500, 2500, 200);

// [depth][isQuietMove][legalMovesSeen]
inline MultiArray<i32, MAX_DEPTH + 1, 2, 256> getLmrTable()
{
    MultiArray<i32, MAX_DEPTH + 1, 2, 256> lmrTable = { };

    for (size_t depth = 1; depth < MAX_DEPTH + 1; depth++)
        for (size_t legalMovesSeen = 1; legalMovesSeen < 256; legalMovesSeen++)
        {
            const double a = ln(static_cast<double>(depth));
            const double b = ln(static_cast<double>(legalMovesSeen));

            lmrTable[depth][false][legalMovesSeen] = static_cast<i32>(round(
                lmrBaseNoisy() + a * b * lmrMulNoisy()
            ));

            lmrTable[depth][true][legalMovesSeen] = static_cast<i32>(round(
                lmrBaseQuiet() + a * b * lmrMulQuiet()
            ));
        }

    return lmrTable;
}

// [depth][isQuietMove][legalMovesSeen]
MAYBE_CONST MultiArray<i32, MAX_DEPTH + 1, 2, 256> LMR_TABLE = getLmrTable();

#if defined(TUNE)
    using TunableParamVariant = std::variant<
        TunableParam<i32>*,
        TunableParam<float>*,
        TunableParam<double>*
    >;

    tsl::ordered_map<std::string, TunableParamVariant> tunableParams =
    {
        { stringify(timeHardPercentage), &timeHardPercentage },
        { stringify(timeSoftPercentage), &timeSoftPercentage },
        { stringify(nodesTmBase),        &nodesTmBase },
        { stringify(nodesTmMul),         &nodesTmMul },
        { stringify(rfpDepthMul),        &rfpDepthMul },
        { stringify(seeNoisyHistMul),    &seeNoisyHistMul },
        { stringify(seeNoisyThreshold),  &seeNoisyThreshold },
        { stringify(seeQuietThreshold),  &seeQuietThreshold },
        { stringify(lmrBaseNoisy),       &lmrBaseNoisy },
        { stringify(lmrBaseQuiet),       &lmrBaseQuiet },
        { stringify(lmrMulNoisy),        &lmrMulNoisy },
        { stringify(lmrMulQuiet),        &lmrMulQuiet },
        { stringify(historyBonusMul),    &historyBonusMul },
        { stringify(historyBonusOffset), &historyBonusOffset },
        { stringify(historyBonusMax),    &historyBonusMax },
        { stringify(historyMalusMul),    &historyMalusMul },
        { stringify(historyMalusOffset), &historyMalusOffset },
        { stringify(historyMalusMax),    &historyMalusMax },
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
                          << ", " << myParam->value
                          << ", " << myParam->min
                          << ", " << myParam->max
                          << ", " << myParam->step
                          << ", 0.002"
                          << "\n";
            }, tunableParam);

            std::flush(std::cout);
        }
    }
#endif
