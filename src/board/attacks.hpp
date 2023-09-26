#include "builtin.hpp"
#include "types.hpp"
#include "utils.hpp"

inline uint64_t pawnAttacks(Square square, Color color)
{
    const int SQUARE_DIAGONAL_LEFT = square + (color == WHITE ? 7 : -9),
              SQUARE_DIAGONAL_RIGHT = square + (color == WHITE ? 9 : -7);

    uint8_t rank = squareRank(square);
    if ((color == BLACK && rank >= 6) || (color == WHITE && rank <= 1))
        return 0ULL;

    char file = squareFile(square);

    if (file == 'a')
        return 1ULL << SQUARE_DIAGONAL_RIGHT;

    if (file == 'h')
        return 1ULL << SQUARE_DIAGONAL_LEFT;

    return (1ULL << SQUARE_DIAGONAL_LEFT) | (1ULL << SQUARE_DIAGONAL_RIGHT);
    
}

inline uint64_t bishopAttacks(Square sq, uint64_t occupied)
{
    uint64_t attacks = 0ULL;
    int r, f;
    int br = sq / 8;
    int bf = sq % 8;

    for (r = br + 1, f = bf + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f++)
    {
        Square s = r*8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (r = br - 1, f = bf + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f++)
    {
        Square s = r*8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (r = br + 1, f = bf - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f--)
    {
        Square s = r*8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (r = br - 1, f = bf - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f--)
    {
        Square s = r*8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    return attacks;
}

inline uint64_t rookAttacks(Square sq, uint64_t occupied)
{
    uint64_t attacks = 0ULL;
    int r, f;
    int rr = sq / 8;
    int rf = sq % 8;

    for (r = rr + 1; r >= 0 && r <= 7; r++)
    {
        Square s = r * 8 + rf;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (r = rr - 1; r >= 0 && r <= 7; r--)
    {
        Square s = r * 8 + rf;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (f = rf + 1; f >= 0 && f <= 7; f++)
    {
        Square s = rr * 8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    for (f = rf - 1; f >= 0 && f <= 7; f--)
    {
        Square s = rr * 8 + f;
        attacks |= (1ULL << s);
        if (occupied & (1ULL << s))
            break;
    }

    return attacks;
}