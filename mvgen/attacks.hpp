#ifndef ATTACKS_HPP
#define ATTACKS_HPP

#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"

uint64_t knightMoves[64], kingMoves[64];

inline void initKnightMoves(Square square)
{
    uint64_t n = 1ULL << square;
    uint64_t h1 = ((n >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL) | ((n << 1ULL) & 0xfefefefefefefefeULL);
    uint64_t h2 = ((n >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL) | ((n << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);
    knightMoves[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
}

inline void initKingMoves(Square square)
{
    char file = squareFile(square);
    uint64_t k = 1ULL << square;
    k |= (k << 8ULL) | (k >> 8ULL);
    k |= ((k & (file != 'a')) >> 1ULL) | ((k & (file != 'h')) << 1ULL);
    kingMoves[square] = k ^ (1ULL << square);
}

inline void initMoves()
{
    for (int square = 0; square < 64; square++)
    {
        initKnightMoves(square);
        initKingMoves(square);
    }
}


#endif