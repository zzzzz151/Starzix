// clang-format off

#pragma once

#include <array>
#include <cassert>

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

    constexpr ArrayVec() = default;

    constexpr T operator[](const std::size_t i) const
    {
        assert(i < mSize);
        return mArr[i];
    }

    constexpr T& operator[](const std::size_t i)
    {
        assert(i < mSize);
        return mArr[i];
    }

    constexpr auto begin() const {
        return mArr.begin();
    }

    constexpr auto end() const {
        return mArr.begin() + mSize;
    }

    constexpr T* ptr(const std::size_t i) const
    {
        assert(i < mSize);
        return &mArr[i];
    }

    constexpr std::size_t size() const { 
        return mSize; 
    }

    constexpr void clear(){ mSize = 0; }

    constexpr void push_back(const T elem) { 
        assert(mSize < N);
        mArr[mSize++] = elem;
    }

    constexpr void pop_back() { 
        assert(mSize > 0);
        mSize--;
    }

    constexpr void swap(const std::size_t i, const std::size_t j) 
    {
        assert(i < mSize && j < mSize);
        std::swap(mArr[i], mArr[j]);
    }

}; // struct ArrayVec
