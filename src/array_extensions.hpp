// clang-format off

#pragma once

#include <array>

// MultiArray

template <typename T, size_t N, size_t... Ns>
struct MultiArrayImpl
{
    using Type = std::array<typename MultiArrayImpl<T, Ns...>::Type, N>;
};

template <typename T, size_t N>
struct MultiArrayImpl<T, N>
{
    using Type = std::array<T, N>;
};

template <typename T, size_t... Ns>
using MultiArray = typename MultiArrayImpl<T, Ns...>::Type;

// ArrayVec

template <typename T, size_t N>
struct ArrayVec
{
    private:

    std::array<T, N> mArr;
    size_t mSize = 0;

    public:

    inline ArrayVec() = default;

    inline T operator[](size_t i) const
    {
        assert(i < mSize);
        return mArr[i];
    }

    inline T& operator[](size_t i)
    {
        assert(i < mSize);
        return mArr[i];
    }

    inline T* ptr(size_t i) const
    {
        assert(i < mSize);
        return &mArr[i];
    }

    inline size_t size() const { 
        return mSize; 
    }

    inline void clear() { mSize = 0; }

    inline void push_back(T elem) { 
        assert(mSize < N);
        mArr[mSize++] = elem;
    }

    inline void swap(size_t i, size_t j) 
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