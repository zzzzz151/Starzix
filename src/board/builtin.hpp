#ifndef BUILTIN_HPP
#define BUILTIN_HPP

#include <cassert>

inline uint8_t lsb(uint64_t b);

inline uint8_t msb(uint64_t b);

#if defined(__GNUC__) // GCC, Clang, ICC
inline uint8_t lsb(uint64_t b)
{
    assert(b);
    return uint8_t(__builtin_ctzll(b));
}

inline uint8_t msb(uint64_t b)
{
    assert(b);
    return uint8_t(63 ^ __builtin_clzll(b));
}

#elif defined(_MSC_VER) // MSVC

#ifdef _WIN64 // MSVC, WIN64
#include <intrin.h>
inline uint8_t lsb(uint64_t b)
{
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return (uint8_t)idx;
}

inline uint8_t msb(uint64_t b)
{
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return (uint8_t)idx;
}

#else // MSVC, WIN32
#include <intrin.h>
inline uint8_t lsb(uint64_t b)
{
    unsigned long idx;

    if (b & 0xffffffff)
    {
        _BitScanForward(&idx, int32_t(b));
        return uint8_t(idx);
    }
    else
    {
        _BitScanForward(&idx, int32_t(b >> 32));
        return uint8_t(idx + 32);
    }
}

inline uint8_t msb(uint64_t b)
{
    unsigned long idx;

    if (b >> 32)
    {
        _BitScanReverse(&idx, int32_t(b >> 32));
        return uint8_t(idx + 32);
    }
    else
    {
        _BitScanReverse(&idx, int32_t(b));
        return uint8_t(idx);
    }
}

#endif

#else // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

inline int popcount(uint64_t mask)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

    return (uint8_t)_mm_popcnt_uint64_t(mask);

#else // Assumed gcc or compatible compiler

    return __builtin_popcountll(mask);

#endif
}

inline uint8_t poplsb(uint64_t &mask)
{
    uint8_t s = lsb(mask);
    mask &= mask - 1; // compiler optimizes this to _blsr_uint64_t
    return uint8_t(s);
}

inline uint64_t pdep(uint64_t val, uint64_t mask) {
    uint64_t res = 0;
    for (uint64_t bb = 1; mask; bb += bb) {
        if (val & bb)
            res |= mask & -mask;
        mask &= mask - 1;
    }
    return res;
}

#endif