// clang-format off

#pragma once

#include <array>
#include <cassert>

// EnumArray

template<typename T, typename... Es>
class EnumArray;

template<typename T, typename E>
class EnumArray<T, E> : public std::array<T, static_cast<std::size_t>(E::Count)>
{
public:

    constexpr T& operator[](const E e)
    {
        return std::array<T, static_cast<std::size_t>(E::Count)>
            ::operator[](static_cast<std::size_t>(e));
    }

    constexpr const T& operator[](const E e) const
    {
        return std::array<T, static_cast<std::size_t>(E::Count)>
            ::operator[](static_cast<std::size_t>(e));
    }

};

template<typename T, typename E, typename... Rest>
class EnumArray<T, E, Rest...>
: public std::array<EnumArray<T, Rest...>, static_cast<std::size_t>(E::Count)>
{
public:

    constexpr EnumArray<T, Rest...>& operator[](const E e)
    {
        return std::array<EnumArray<T, Rest...>, static_cast<std::size_t>(E::Count)>
            ::operator[](static_cast<std::size_t>(e));
    }

    constexpr const EnumArray<T, Rest...>& operator[](const E e) const
    {
        return std::array<EnumArray<T, Rest...>, static_cast<std::size_t>(E::Count)>
            ::operator[](static_cast<std::size_t>(e));
    }

};

// MultiArray

template <typename T, std::size_t N, std::size_t... Ns>
struct MultiArrayImpl
{
    using Type = std::array<typename MultiArrayImpl<T, Ns...>::Type, N>;
};

template <typename T, std::size_t N>
struct MultiArrayImpl<T, N>
{
    using Type = std::array<T, N>;
};

template <typename T, std::size_t... Ns>
using MultiArray = typename MultiArrayImpl<T, Ns...>::Type;

// ArrayVec

template <typename T, std::size_t N>
struct ArrayVec
{
private:
    std::array<T, N> mArr;
    std::size_t mSize = 0;

public:

    constexpr T operator[](const std::size_t i) const
    {
        assert(i < mSize);
        return mArr[i];
    }

    constexpr const T& operator[](const std::size_t i)
    {
        assert(i < mSize);
        return mArr[i];
    }

    constexpr T* ptr(const std::size_t i) const
    {
        assert(i < mSize);
        return &mArr[i];
    }

    constexpr auto begin() const {
        return mArr.begin();
    }

    constexpr auto end() const {
        return mArr.begin() + static_cast<std::ptrdiff_t>(mSize);
    }

    constexpr std::size_t size() const {
        return mSize;
    }

    constexpr void clear() {
        mSize = 0;
    }

    constexpr void push_back(const T x)
    {
        assert(mSize < N);
        mArr[mSize++] = x;
    }

    constexpr void pop_back()
    {
        assert(mSize > 0);
        mSize--;
    }

    constexpr void swap(const std::size_t i, const std::size_t j)
    {
        assert(i < mSize && j < mSize);
        std::swap(mArr[i], mArr[j]);
    }

    constexpr bool contains(const T x) const
    {
        for (const T elem : *this)
            if (elem == x)
                return true;

        return false;
    }

}; // struct ArrayVec
