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

    inline ArrayVec() = default;

    inline T operator[](const std::size_t i) const
    {
        assert(i < mSize);
        return mArr[i];
    }

    inline T& operator[](const std::size_t i)
    {
        assert(i < mSize);
        return mArr[i];
    }

    inline T* ptr(const std::size_t i) const
    {
        assert(i < mSize);
        return &mArr[i];
    }

    inline std::size_t size() const { 
        return mSize; 
    }

    inline void clear(){ mSize = 0; }

    inline void push_back(const T elem) { 
        assert(mSize < N);
        mArr[mSize++] = elem;
    }

    inline void pop_back() { 
        assert(mSize > 0);
        mSize--;
    }

    inline void swap(const std::size_t i, const std::size_t j) 
    {
        assert(i < mSize && j < mSize);
        std::swap(mArr[i], mArr[j]);
    }

    inline auto begin() const {
        return mArr.begin();
    }

    inline auto end() const {
        return mArr.begin() + mSize;
    }

}; // struct ArrayVec
