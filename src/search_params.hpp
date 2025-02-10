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

constexpr i32 MAX_DEPTH = 100;
constexpr i32 INF = 30'000;
constexpr i32 MIN_MATE_SCORE = INF - MAX_DEPTH;

// History heuristic
constexpr i32 HISTORY_MAX = 16384;
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMul    = TunableParam<i32>(300, 50, 600, 25);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusOffset = TunableParam<i32>(0, 0, 500, 100);
MAYBE_CONSTEXPR TunableParam<i32> historyBonusMax    = TunableParam<i32>(1500, 500, 2500, 200);

#if defined(TUNE)
    using TunableParamVariant = std::variant<
        TunableParam<i32>*
    >;

    tsl::ordered_map<std::string, TunableParamVariant> tunableParams = {
        { stringify(historyBonusMul),    &historyBonusMul },
        { stringify(historyBonusOffset), &historyBonusOffset },
        { stringify(historyBonusMax),    &historyBonusMax },
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
                          << std::endl;
            }, tunableParam);
        }
    }
#endif
