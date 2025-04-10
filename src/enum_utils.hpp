// clang-format off

#pragma once

#include "types.hpp"
#include <stdexcept>
#include <algorithm>

template <typename E>
constexpr E asEnum(const auto x)
{
    return x >= 0 && static_cast<size_t>(x) < static_cast<size_t>(E::Count)
         ? static_cast<E>(x)
         : throw std::logic_error("Argument doesn't match an enum option");
}

template <typename E>
constexpr E add(const E a, const auto b)
{
    return asEnum<E>(static_cast<i64>(a) + static_cast<i64>(b));
}

template <typename E>
constexpr E addAndClamp(const E a, const auto b)
{
    const i64 sum = static_cast<i64>(a) + static_cast<i64>(b);
    const i64 clamped = std::clamp<i64>(sum, static_cast<i64>(0), static_cast<i64>(E::Count) - 1);
    return asEnum<E>(clamped);
}

template <typename E>
constexpr E next(const E x)
{
    return add<E>(x, 1);
}

template <typename E>
constexpr E previous(const E x)
{
    return add<E>(x, -1);
}

template <typename E>
class EnumIter
{
public:

    class Iter
    {
    private:

        size_t mValue = 0;

    public:

        constexpr Iter (const size_t value) : mValue(value) { }

        constexpr const E operator*() const
        {
            return asEnum<E>(mValue);
        }

        constexpr Iter& operator++()
        {
            ++mValue;
            return *this;
        }

        constexpr bool operator!=(const Iter& other) const
        {
            return mValue != other.mValue;
        }

    }; // class Iter

    constexpr static Iter begin() {
        return Iter(0);
    }

    constexpr static Iter end() {
        return Iter(static_cast<size_t>(E::Count));
    }

}; // class EnumIter
