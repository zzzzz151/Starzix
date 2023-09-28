#ifndef ATTACKS_HPP
#define ATTACKS_HPP

// clang-format off
#include "builtin.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace attacks
{
    namespace internal
    {
        // private stuff

        uint64_t knightAttacks[64], kingAttacks[64];

        inline void initKnightAttacks(Square square)
        {
            uint64_t n = 1ULL << square;
            uint64_t h1 = ((n >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL) | ((n << 1ULL) & 0xfefefefefefefefeULL);
            uint64_t h2 = ((n >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL) | ((n << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);
            knightAttacks[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
        }

        inline uint64_t bishopAttacksSlow(Square sq, uint64_t occupied, bool excludeLastSquare = false)
        {
            uint64_t attacks = 0ULL;
            int r, f;
            int br = sq / 8;
            int bf = sq % 8;

            for (r = br + 1, f = bf + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f++)
            {
                if (excludeLastSquare && (f == 7 || r == 7))
                    break;
                Square s = r * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (r = br - 1, f = bf + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f++)
            {
                if (excludeLastSquare && (r == 0 || f == 7))
                    break;
                Square s = r * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (r = br + 1, f = bf - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f--)
            {
                if (excludeLastSquare && (r == 7 || f == 0))
                    break;
                Square s = r * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (r = br - 1, f = bf - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f--)
            {
                if (excludeLastSquare && (r == 0 || f == 0))
                    break;
                Square s = r * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            return attacks;
        }

        inline uint64_t rookAttacksSlow(Square sq, uint64_t occupied, bool excludeLastSquare = false)
        {
            uint64_t attacks = 0ULL;
            int r, f;
            int rr = sq / 8;
            int rf = sq % 8;

            for (r = rr + 1; r >= 0 && r <= 7; r++)
            {
                if (excludeLastSquare && r == 7)
                    break;
                Square s = r * 8 + rf;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (r = rr - 1; r >= 0 && r <= 7; r--)
            {
                if (excludeLastSquare && r == 0)
                    break;
                Square s = r * 8 + rf;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (f = rf + 1; f >= 0 && f <= 7; f++)
            {
                if (excludeLastSquare && f == 7)
                    break;
                Square s = rr * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            for (f = rf - 1; f >= 0 && f <= 7; f--)
            {
                if (excludeLastSquare && f == 0)
                    break;
                Square s = rr * 8 + f;
                attacks |= (1ULL << s);
                if (occupied & (1ULL << s))
                    break;
            }

            return attacks;
        }

        const uint64_t BISHOP_SHIFTS[64] = {
            58, 59, 59, 59, 59, 59, 59, 58, 
            59, 59, 59, 59, 59, 59, 59, 59, 
            59, 59, 57, 57, 57, 57, 59, 59, 
            59, 59, 57, 55, 55, 57, 59, 59, 
            59, 59, 57, 55, 55, 57, 59, 59, 
            59, 59, 57, 57, 57, 57, 59, 59, 
            59, 59, 59, 59, 59, 59, 59, 59, 
            58, 59, 59, 59, 59, 59, 59, 58
        };

        const uint64_t ROOK_SHIFTS[64] = {
            52, 53, 53, 53, 53, 53, 53, 52, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            53, 54, 54, 54, 54, 54, 54, 53, 
            52, 53, 53, 53, 53, 53, 53, 52
        };

        const uint64_t ROOK_MAGICS[64] = {
            0xa8002c000108020ULL, 0x6c00049b0002001ULL, 0x100200010090040ULL, 0x2480041000800801ULL, 
            0x280028004000800ULL, 0x900410008040022ULL, 0x280020001001080ULL, 0x2880002041000080ULL,
            0xa000800080400034ULL, 0x4808020004000ULL, 0x2290802004801000ULL, 0x411000d00100020ULL,
            0x402800800040080ULL, 0xb000401004208ULL, 0x2409000100040200ULL, 0x1002100004082ULL,
            0x22878001e24000ULL, 0x1090810021004010ULL, 0x801030040200012ULL, 0x500808008001000ULL,
            0xa08018014000880ULL, 0x8000808004000200ULL, 0x201008080010200ULL, 0x801020000441091ULL,
            0x800080204005ULL, 0x1040200040100048ULL, 0x120200402082ULL, 0xd14880480100080ULL,
            0x12040280080080ULL, 0x100040080020080ULL, 0x9020010080800200ULL, 0x813241200148449ULL,
            0x491604001800080ULL, 0x100401000402001ULL, 0x4820010021001040ULL, 0x400402202000812ULL,
            0x209009005000802ULL, 0x810800601800400ULL, 0x4301083214000150ULL, 0x204026458e001401ULL,
            0x40204000808000ULL, 0x8001008040010020ULL, 0x8410820820420010ULL, 0x1003001000090020ULL,
            0x804040008008080ULL, 0x12000810020004ULL, 0x1000100200040208ULL, 0x430000a044020001ULL,
            0x280009023410300ULL, 0xe0100040002240ULL, 0x200100401700ULL, 0x2244100408008080ULL,
            0x8000400801980ULL, 0x2000810040200ULL, 0x8010100228810400ULL, 0x2000009044210200ULL,
            0x4080008040102101ULL, 0x40002080411d01ULL, 0x2005524060000901ULL, 0x502001008400422ULL,
            0x489a000810200402ULL, 0x1004400080a13ULL, 0x4000011008020084ULL, 0x26002114058042ULL,
        };

        const uint64_t BISHOP_MAGICS[64] = {
            0x89a1121896040240ULL, 0x2004844802002010ULL, 0x2068080051921000ULL, 0x62880a0220200808ULL,
            0x4042004000000ULL, 0x100822020200011ULL, 0xc00444222012000aULL, 0x28808801216001ULL,
            0x400492088408100ULL, 0x201c401040c0084ULL, 0x840800910a0010ULL, 0x82080240060ULL,
            0x2000840504006000ULL, 0x30010c4108405004ULL, 0x1008005410080802ULL, 0x8144042209100900ULL,
            0x208081020014400ULL, 0x4800201208ca00ULL, 0xf18140408012008ULL, 0x1004002802102001ULL,
            0x841000820080811ULL, 0x40200200a42008ULL, 0x800054042000ULL, 0x88010400410c9000ULL,
            0x520040470104290ULL, 0x1004040051500081ULL, 0x2002081833080021ULL, 0x400c00c010142ULL,
            0x941408200c002000ULL, 0x658810000806011ULL, 0x188071040440a00ULL, 0x4800404002011c00ULL,
            0x104442040404200ULL, 0x511080202091021ULL, 0x4022401120400ULL, 0x80c0040400080120ULL,
            0x8040010040820802ULL, 0x480810700020090ULL, 0x102008e00040242ULL, 0x809005202050100ULL,
            0x8002024220104080ULL, 0x431008804142000ULL, 0x19001802081400ULL, 0x200014208040080ULL,
            0x3308082008200100ULL, 0x41010500040c020ULL, 0x4012020c04210308ULL, 0x208220a202004080ULL,
            0x111040120082000ULL, 0x6803040141280a00ULL, 0x2101004202410000ULL, 0x8200000041108022ULL,
            0x21082088000ULL, 0x2410204010040ULL, 0x40100400809000ULL, 0x822088220820214ULL,
            0x40808090012004ULL, 0x910224040218c9ULL, 0x402814422015008ULL, 0x90014004842410ULL,
            0x1000042304105ULL, 0x10008830412a00ULL, 0x2520081090008908ULL, 0x40102000a0a60140ULL,
        };

        uint64_t bishopAttacksEmptyBoard[64], rookAttacksEmptyBoard[64],
                 bishopAttacksTable[64][1ULL << 9ULL], rookAttacksTable[64][1ULL << 12ULL];

    }

    inline void initAttacks()
    {
        using namespace internal;

        // Init king and knight attacks
        uint64_t king = 1;
        for (int square = 0; square < 64; square++)
        {
            initKnightAttacks(square);

            uint64_t attacks = shiftLeft(king) | shiftRight(king) | king;
            attacks = (attacks | shiftUp(attacks) | shiftDown(attacks)) ^ king;
            internal::kingAttacks[square] = attacks;
            king <<= 1ULL;
        }

        // Init slider attacks on an empty board
        for (Square sq = 0; sq < 64; sq++)
        {
            // last arg true = exclude last square of each direction
            bishopAttacksEmptyBoard[sq] = bishopAttacksSlow(sq, 0ULL, true);
            rookAttacksEmptyBoard[sq] = rookAttacksSlow(sq, 0ULL, true);
        }

        // Init slider tables
        for (Square sq = 0; sq < 64; sq++)
        {
            // Bishop
            uint64_t numBlockersArrangements = 1ULL << popcount(bishopAttacksEmptyBoard[sq]);
            for (uint64_t n = 0; n < numBlockersArrangements; n++)
            {
                uint64_t blockersArrangement = pdep(n, bishopAttacksEmptyBoard[sq]);
                uint64_t key = (blockersArrangement * BISHOP_MAGICS[sq]) >> BISHOP_SHIFTS[sq];
                bishopAttacksTable[sq][key] = bishopAttacksSlow(sq, blockersArrangement);
            }

            // Rook
            numBlockersArrangements = 1ULL << popcount(rookAttacksEmptyBoard[sq]);
            for (uint64_t n = 0; n < numBlockersArrangements; n++)
            {
                uint64_t blockersArrangement = pdep(n, rookAttacksEmptyBoard[sq]);
                uint64_t key = (blockersArrangement * ROOK_MAGICS[sq]) >> ROOK_SHIFTS[sq];
                rookAttacksTable[sq][key] = rookAttacksSlow(sq, blockersArrangement);
            }
        }

    }

    inline uint64_t pawnAttacks(Square square, Color color)
    {
        const int SQUARE_DIAGONAL_LEFT = square + (color == WHITE ? 7 : -9),
                  SQUARE_DIAGONAL_RIGHT = square + (color == WHITE ? 9 : -7);

        uint8_t rank = squareRank(square);
        if ((color == WHITE && rank >= 6) || (color == BLACK && rank <= 1))
            return 0ULL;

        uint8_t file = squareFile(square);

        if (file == 0)
            return 1ULL << SQUARE_DIAGONAL_RIGHT;

        if (file == 7)
            return 1ULL << SQUARE_DIAGONAL_LEFT;

        return (1ULL << SQUARE_DIAGONAL_LEFT) | (1ULL << SQUARE_DIAGONAL_RIGHT);
    }

    inline uint64_t knightAttacks(Square square) 
    { 
        return internal::knightAttacks[square];
     }

    inline uint64_t kingAttacks(Square square) 
    {        
        return internal::kingAttacks[square]; 
    }

    inline uint64_t bishopAttacks(Square square, uint64_t occ)
    {
        using namespace internal;

        // Mask to only include bits on diagonals
        uint64_t blockers = occ & bishopAttacksEmptyBoard[square];

        // Generate the key using a multiplication and right shift
        uint64_t key = (blockers * BISHOP_MAGICS[square]) >> BISHOP_SHIFTS[square];

        // Return the preinitialized attack set bitboard from the table
        return bishopAttacksTable[square][key];
    }

    inline uint64_t rookAttacks(Square square, uint64_t occ)
    {
        using namespace internal;

        // Mask to only include bits on rank and file
        uint64_t blockers = occ & rookAttacksEmptyBoard[square];

        // Generate the key using a multiplication and right shift
        uint64_t key = (blockers * ROOK_MAGICS[square]) >> ROOK_SHIFTS[square];

        // Return the preinitialized attack set bitboard from the table
        return rookAttacksTable[square][key];
    }

}

#endif