// clang-format off

#pragma once

#include "utils.hpp"

namespace internal {
    
// [color][square]
constexpr MultiArray<u64, 2, 64> PAWN_ATTACKS = []() consteval
{
    MultiArray<u64, 2, 64> pawnAttacks;

    auto getPawnAttacks = [](const Square square, const Color color) consteval -> u64
    {   
        const Rank rank = squareRank(square);

        if ((color == Color::WHITE && rank == Rank::RANK_8) 
        ||  (color == Color::BLACK && rank == Rank::RANK_1))
            return 0;

        const File file = squareFile(square);

        const int squareDiagonalLeft  = (int)square + (color == Color::WHITE ? 7 : -9);
        const int squareDiagonalRight = (int)square + (color == Color::WHITE ? 9 : -7);

        return file == File::A ? bitboard(squareDiagonalRight)
             : file == File::H ? bitboard(squareDiagonalLeft)
             : bitboard(squareDiagonalLeft) | bitboard(squareDiagonalRight);
    };

    for (const Color color : {Color::WHITE, Color::BLACK})
        for (Square square = 0; square < 64; square++)
            pawnAttacks[(int)color][square] = getPawnAttacks(square, color);

    return pawnAttacks;
}();

// [square]
constexpr std::array<u64, 64> KNIGHT_ATTACKS = []() consteval
{
    std::array<u64, 64> knightAttacks;

    for (Square square = 0; square < 64; square++)
    {
        const u64 sqBb = bitboard(square);
        const u64 h1 = ((sqBb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL) | ((sqBb << 1ULL) & 0xfefefefefefefefeULL);
        const u64 h2 = ((sqBb >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL) | ((sqBb << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);
        knightAttacks[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
    }

    return knightAttacks;
}();

// [square]
constexpr std::array<u64, 64> KING_ATTACKS = []() consteval
{
    std::array<u64, 64> kingAttacks;

    for (Square square = 0; square < 64; square++)
    {
        const u64 sqBb = bitboard(square);
        kingAttacks[square] = sqBb;
        kingAttacks[square] |= (sqBb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL; // shifted left
        kingAttacks[square] |= (sqBb << 1ULL) & 0xfefefefefefefefeULL; // shifted right
        kingAttacks[square] |= kingAttacks[square] << 8ULL; // shifted up
        kingAttacks[square] |= kingAttacks[square] >> 8ULL; // shifted down
        kingAttacks[square] ^= sqBb;
    }

    return kingAttacks;
}();

constexpr u64 bishopAttacksSlow(const Square sq, const u64 occupied)
{
    u64 attacks = 0ULL;
    int r, f;
    const int rank = sq / 8;
    const int file = sq % 8;

    for (r = rank + 1, f = file + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f++)
    {
        const Square sq = r * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (r = rank - 1, f = file + 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f++)
    {
        const Square sq = r * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (r = rank + 1, f = file - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r++, f--)
    {
        const Square sq = r * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (r = rank - 1, f = file - 1; r >= 0 && r <= 7 && f >= 0 && f <= 7; r--, f--)
    {
        const Square sq = r * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    return attacks;
}

constexpr u64 rookAttacksSlow(const Square sq, const u64 occupied)
{
    u64 attacks = 0ULL;
    int r, f;
    const int rank = sq / 8;
    const int file = sq % 8;

    for (r = rank + 1; r >= 0 && r <= 7; r++)
    {
        const Square sq = r * 8 + file;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (r = rank - 1; r >= 0 && r <= 7; r--)
    {
        const Square sq = r * 8 + file;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (f = file + 1; f >= 0 && f <= 7; f++)
    {
        const Square sq = rank * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    for (f = file - 1; f >= 0 && f <= 7; f--)
    {
        const Square sq = rank * 8 + f;
        attacks |= bitboard(sq);

        if (occupied & bitboard(sq))
            break;
    }

    return attacks;
}

// [square]
constexpr std::array<u64, 64> BISHOP_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR = []() consteval
{
    std::array<u64, 64> bishopAttacks;
    constexpr u64 NO_EDGES_BB = 0x7e7e7e7e7e7e00ULL;

    for (Square square = 0; square < 64; square++)
        bishopAttacks[square] = bishopAttacksSlow(square, 0) & NO_EDGES_BB;

    return bishopAttacks;
}();

// [square]
constexpr std::array<u64, 64> ROOK_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR = []() consteval
{
    std::array<u64, 64> rookAttacks;

    for (Square square = 0; square < 64; square++)
    {
        rookAttacks[square] = rookAttacksSlow(square, 0);

        // remove corners
        rookAttacks[square] &= ~(bitboard(0) | bitboard(7) | bitboard(56) | bitboard(63));

        const Rank rank = squareRank(square);
        const File file = squareFile(square);

        if (rank != Rank::RANK_1) 
            rookAttacks[square] &= ~RANK_BB[(int)Rank::RANK_1];

        if (rank != Rank::RANK_8) 
            rookAttacks[square] &= ~RANK_BB[(int)Rank::RANK_8];

        if (file != File::A) 
            rookAttacks[square] &= ~FILE_BB[(int)File::A];

        if (file != File::H) 
    	    rookAttacks[square] &= ~FILE_BB[(int)File::H];
    }

    return rookAttacks;
}();

// [square]
constexpr u64 BISHOP_SHIFTS[64] = {
    58, 59, 59, 59, 59, 59, 59, 58, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    58, 59, 59, 59, 59, 59, 59, 58
};

// [square]
constexpr u64 ROOK_SHIFTS[64] = {
    52, 53, 53, 53, 53, 53, 53, 52, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    52, 53, 53, 53, 53, 53, 53, 52
};

// [square]
constexpr u64 BISHOP_MAGICS[64] = {
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

// [square]
constexpr u64 ROOK_MAGICS[64] = {
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

// [square][index]
constexpr MultiArray<u64, 64, 1ULL << 9ULL> BISHOP_ATTACKS_TABLE = []() consteval
{    
    MultiArray<u64, 64, 1ULL << 9ULL> bishopAttacksTable = { };

    for (Square sq = 0; sq < 64; sq++)
    {
        const u64 numBlockersArrangements = 1ULL << std::popcount(BISHOP_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[sq]);

        for (u64 n = 0; n < numBlockersArrangements; n++)
        {
            const u64 blockersArrangement = pdep(n, BISHOP_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[sq]);
            const u64 index = (blockersArrangement * BISHOP_MAGICS[sq]) >> BISHOP_SHIFTS[sq];
            bishopAttacksTable[sq][index] = bishopAttacksSlow(sq, blockersArrangement);
        }
    }

    return bishopAttacksTable;
}();

// [square][index]
constexpr MultiArray<u64, 64, 1ULL << 12ULL> ROOK_ATTACKS_TABLE = []() consteval
{
    MultiArray<u64, 64, 1ULL << 12ULL> rookAttacksTable = { };

    for (Square sq = 0; sq < 64; sq++)
    {
        const u64 numBlockersArrangements = 1ULL << std::popcount(ROOK_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[sq]);

        for (u64 n = 0; n < numBlockersArrangements; n++)
        {
            const u64 blockersArrangement = pdep(n, ROOK_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[sq]);
            const u64 index = (blockersArrangement * ROOK_MAGICS[sq]) >> ROOK_SHIFTS[sq];
            rookAttacksTable[sq][index] = rookAttacksSlow(sq, blockersArrangement);
        }
    }

    return rookAttacksTable;
}();

} // namespace internal

constexpr u64 getPawnAttacks(const Square square, const Color color) {
    return internal::PAWN_ATTACKS[(int)color][square];
}

constexpr u64 getKnightAttacks(const Square square)  { 
    return internal::KNIGHT_ATTACKS[square];
}

constexpr u64 getBishopAttacks(const Square square, const u64 occupancy)
{
    using namespace internal;
    const u64 blockers = occupancy & BISHOP_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[square];
    const u64 index = (blockers * BISHOP_MAGICS[square]) >> BISHOP_SHIFTS[square];
    return BISHOP_ATTACKS_TABLE[square][index];
}

constexpr u64 getRookAttacks(const Square square, const u64 occupancy)
{
    using namespace internal;
    const u64 blockers = occupancy & ROOK_ATKS_EMPTY_BOARD_EXCLUDING_LAST_SQ_EACH_DIR[square];
    const u64 index = (blockers * ROOK_MAGICS[square]) >> ROOK_SHIFTS[square];
    return ROOK_ATTACKS_TABLE[square][index];
}

constexpr u64 getQueenAttacks(const Square square, const u64 occupancy) {
    return getBishopAttacks(square, occupancy) | getRookAttacks(square, occupancy);
}

constexpr u64 getKingAttacks(const Square square)  {        
    return internal::KING_ATTACKS[square];
}
