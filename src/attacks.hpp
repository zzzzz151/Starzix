// clang-format off

#pragma once

#include "utils.hpp"

namespace internal {

constexpr EnumArray<Bitboard, Color, Square> PAWN_ATTACKS = [] () consteval
{
    EnumArray<Bitboard, Color, Square> pawnAttacks = { };

    constexpr auto getPawnAttacks = [] (
        const Square square, const Color color) consteval -> Bitboard
    {
        const Rank rank = squareRank(square);

        if ((color == Color::White && rank == Rank::Rank8)
        ||  (color == Color::Black && rank == Rank::Rank1))
            return 0;

        const File file = squareFile(square);

        Square squareDiagonalLeft, squareDiagonalRight;

        if (file != File::A)
            squareDiagonalLeft = add<Square>(square, color == Color::White ? 7 : -9);

        if (file != File::H)
            squareDiagonalRight = add<Square>(square, color == Color::White ? 9 : -7);

        return file == File::A ? squareBb(squareDiagonalRight)
             : file == File::H ? squareBb(squareDiagonalLeft)
             : squareBb(squareDiagonalLeft) | squareBb(squareDiagonalRight);
    };

    for (const Color color : EnumIter<Color>())
        for (const Square square : EnumIter<Square>())
            pawnAttacks[color][square] = getPawnAttacks(square, color);

    return pawnAttacks;
}();

constexpr EnumArray<Bitboard, Square> KNIGHT_ATTACKS = [] () consteval
{
    EnumArray<Bitboard, Square> knightAttacks;

    for (const Square square : EnumIter<Square>())
    {
        const Bitboard sqBb = squareBb(square);

        const u64 h1 = ((sqBb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL)
                     | ((sqBb << 1ULL) & 0xfefefefefefefefeULL);

        const u64 h2 = ((sqBb >> 2ULL) & 0x3f3f3f3f3f3f3f3fULL)
                     | ((sqBb << 2ULL) & 0xfcfcfcfcfcfcfcfcULL);

        knightAttacks[square] = (h1 << 16ULL) | (h1 >> 16ULL) | (h2 << 8ULL) | (h2 >> 8ULL);
    }

    return knightAttacks;
}();

constexpr EnumArray<Bitboard, Square> KING_ATTACKS = [] () consteval
{
    EnumArray<Bitboard, Square> kingAttacks;

    for (const Square square : EnumIter<Square>())
    {
        const Bitboard sqBb = squareBb(square);
        kingAttacks[square] = sqBb;
        kingAttacks[square] |= (sqBb >> 1ULL) & 0x7f7f7f7f7f7f7f7fULL; // Shifted left
        kingAttacks[square] |= (sqBb << 1ULL) & 0xfefefefefefefefeULL; // Shifted right
        kingAttacks[square] |= kingAttacks[square] << 8ULL; // Shifted up
        kingAttacks[square] |= kingAttacks[square] >> 8ULL; // Shifted down
        kingAttacks[square] ^= sqBb;
    }

    return kingAttacks;
}();

consteval Bitboard bishopAttacksSlow(const Square square, const Bitboard occupied)
{
    Bitboard attacks = 0;

    const File file = squareFile(square);
    const Rank rank = squareRank(square);

    File f;
    Rank r;

    // Top right
    for (f = file, r = rank; f != File::H && r != Rank::Rank8; )
    {
        f = next<File>(f);
        r = next<Rank>(r);

        const Square targetSquare = toSquare(f, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Top left
    for (f = file, r = rank; f != File::A && r != Rank::Rank8; )
    {
        f = previous<File>(f);
        r = next<Rank>(r);

        const Square targetSquare = toSquare(f, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Down right
    for (f = file, r = rank; f != File::H && r != Rank::Rank1; )
    {
        f = next<File>(f);
        r = previous<Rank>(r);

        const Square targetSquare = toSquare(f, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Down left
    for (f = file, r = rank; f != File::A && r != Rank::Rank1; )
    {
        f = previous<File>(f);
        r = previous<Rank>(r);

        const Square targetSquare = toSquare(f, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    return attacks;
}

consteval Bitboard rookAttacksSlow(const Square square, const Bitboard occupied)
{
    Bitboard attacks = 0;

    const File file = squareFile(square);
    const Rank rank = squareRank(square);

    // Right
    for (File f = file; f != File::H; )
    {
        f = next<File>(f);

        const Square targetSquare = toSquare(f, rank);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Left
    for (File f = file; f != File::A; )
    {
        f = previous<File>(f);

        const Square targetSquare = toSquare(f, rank);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Up
    for (Rank r = rank; r != Rank::Rank8; )
    {
        r = next<Rank>(r);

        const Square targetSquare = toSquare(file, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    // Down
    for (Rank r = rank; r != Rank::Rank1; )
    {
        r = previous<Rank>(r);

        const Square targetSquare = toSquare(file, r);
        attacks |= squareBb(targetSquare);

        if (hasSquare(occupied, targetSquare))
            break;
    }

    return attacks;
}

struct MagicEntry
{
public:
    Bitboard attacksEmptyBoardNoEdges;
    u64 magic;
    u64 shift;
};

constexpr EnumArray<MagicEntry, Square> BISHOP_MAGIC_ENTRIES = [] () consteval
{
    constexpr EnumArray<u64, Square> BISHOP_MAGICS = {
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

    EnumArray<MagicEntry, Square> bishopMagicEntries;

    for (const Square square : EnumIter<Square>())
    {
        MagicEntry& magicEntry = bishopMagicEntries[square];

        // Remove edges from attacks
        constexpr Bitboard NO_EDGES_BB = 0x7e7e7e7e7e7e00ULL;
        magicEntry.attacksEmptyBoardNoEdges = bishopAttacksSlow(square, 0) & NO_EDGES_BB;

        magicEntry.magic = BISHOP_MAGICS[square];

        magicEntry.shift = static_cast<u64>(
            64 - std::popcount(magicEntry.attacksEmptyBoardNoEdges)
        );
    }

    return bishopMagicEntries;
}();

constexpr EnumArray<MagicEntry, Square> ROOK_MAGIC_ENTRIES = [] () consteval
{
    constexpr EnumArray<u64, Square> ROOK_MAGICS = {
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

    EnumArray<MagicEntry, Square> rookMagicEntries;

    for (const Square square : EnumIter<Square>())
    {
        MagicEntry& magicEntry = rookMagicEntries[square];

        magicEntry.attacksEmptyBoardNoEdges = rookAttacksSlow(square, 0);

        // Remove corners
        constexpr Bitboard NO_CORNERS_BB = 0x7effffffffffff7eULL;
        magicEntry.attacksEmptyBoardNoEdges &= NO_CORNERS_BB;

        const File file = squareFile(square);
        const Rank rank = squareRank(square);

        if (file != File::A)
            magicEntry.attacksEmptyBoardNoEdges &= ~fileBb(File::A);

        if (file != File::H)
            magicEntry.attacksEmptyBoardNoEdges &= ~fileBb(File::H);

        if (rank != Rank::Rank1)
            magicEntry.attacksEmptyBoardNoEdges &= ~rankBb(Rank::Rank1);

        if (rank != Rank::Rank8)
            magicEntry.attacksEmptyBoardNoEdges &= ~rankBb(Rank::Rank8);

        magicEntry.magic = ROOK_MAGICS[square];

        magicEntry.shift = static_cast<u64>(
            64 - std::popcount(magicEntry.attacksEmptyBoardNoEdges)
        );
    }

    return rookMagicEntries;
}();

// [square][index]
constexpr MultiArray<Bitboard, 64, 1ULL << 9> BISHOP_ATTACKS_TABLE = [] () consteval
{
    MultiArray<Bitboard, 64, 1ULL << 9> bishopAttacksTable = { };

    for (const Square square : EnumIter<Square>())
    {
        const MagicEntry& magicEntry = BISHOP_MAGIC_ENTRIES[square];

        const u64 numBlockersArrangements
            = 1ULL << std::popcount(magicEntry.attacksEmptyBoardNoEdges);

        for (size_t n = 0; n < numBlockersArrangements; n++)
        {
            const Bitboard blockers = pdep(n, magicEntry.attacksEmptyBoardNoEdges);
            const size_t idx = (blockers * magicEntry.magic) >> magicEntry.shift;

            bishopAttacksTable[static_cast<size_t>(square)][idx]
                = bishopAttacksSlow(square, blockers);
        }
    }

    return bishopAttacksTable;
}();

// [square][index]
constexpr MultiArray<Bitboard, 64, 1ULL << 12> ROOK_ATTACKS_TABLE = [] () consteval
{
    MultiArray<Bitboard, 64, 1ULL << 12> rookAttacksTable = { };

    for (const Square square : EnumIter<Square>())
    {
        const MagicEntry& magicEntry = ROOK_MAGIC_ENTRIES[square];

        const u64 numBlockersArrangements
            = 1ULL << std::popcount(magicEntry.attacksEmptyBoardNoEdges);

        for (size_t n = 0; n < numBlockersArrangements; n++)
        {
            const Bitboard blockers = pdep(n, magicEntry.attacksEmptyBoardNoEdges);
            const size_t idx = (blockers * magicEntry.magic) >> magicEntry.shift;

            rookAttacksTable[static_cast<size_t>(square)][idx]
                = rookAttacksSlow(square, blockers);
        }
    }

    return rookAttacksTable;
}();

} // namespace internal

constexpr Bitboard getPawnAttacks(const Square square, const Color color)
{
    return internal::PAWN_ATTACKS[color][square];
}

constexpr Bitboard getKnightAttacks(const Square square)
{
    return internal::KNIGHT_ATTACKS[square];
}

constexpr Bitboard getBishopAttacks(const Square square, const Bitboard occupied)
{
    using namespace internal;

    const MagicEntry& magicEntry = BISHOP_MAGIC_ENTRIES[square];

    const Bitboard blockers = occupied & magicEntry.attacksEmptyBoardNoEdges;
    const size_t idx = (blockers * magicEntry.magic) >> magicEntry.shift;

    return BISHOP_ATTACKS_TABLE[static_cast<size_t>(square)][idx];
}

constexpr Bitboard getRookAttacks(const Square square, const Bitboard occupied)
{
    using namespace internal;

    const MagicEntry& magicEntry = ROOK_MAGIC_ENTRIES[square];

    const Bitboard blockers = occupied & magicEntry.attacksEmptyBoardNoEdges;
    const size_t idx = (blockers * magicEntry.magic) >> magicEntry.shift;

    return ROOK_ATTACKS_TABLE[static_cast<size_t>(square)][idx];
}

constexpr Bitboard getQueenAttacks(const Square square, const Bitboard occupied)
{
    return getBishopAttacks(square, occupied) | getRookAttacks(square, occupied);
}

constexpr Bitboard getKingAttacks(const Square square)
{
    return internal::KING_ATTACKS[square];
}

// [from][to]
constexpr EnumArray<Bitboard, Square, Square> BETWEEN_EXCLUSIVE_BB = [] () consteval
{
    EnumArray<Bitboard, Square, Square> betweenExclusiveBb = { };

    for (const Square sq1 : EnumIter<Square>()) {
        for (const Square sq2 : EnumIter<Square>())
        {
            if (sq1 == sq2) continue;

            if (hasSquare(getBishopAttacks(sq1, 0), sq2))
            {
                betweenExclusiveBb[sq1][sq2] = getBishopAttacks(sq1, squareBb(sq2))
                                             & getBishopAttacks(sq2, squareBb(sq1));
            }
            else if (hasSquare(getRookAttacks(sq1, 0), sq2))
            {
                betweenExclusiveBb[sq1][sq2] = getRookAttacks(sq1, squareBb(sq2))
                                             & getRookAttacks(sq2, squareBb(sq1));
            }
        }
    }

    return betweenExclusiveBb;
}();

// [from][to]
constexpr EnumArray<Bitboard, Square, Square> LINE_THRU_BB = [] () consteval
{
    EnumArray<Bitboard, Square, Square> lineThruBb = { };

    for (const Square sq1 : EnumIter<Square>()) {
        for (const Square sq2 : EnumIter<Square>())
        {
            if (sq1 == sq2) continue;

            if (hasSquare(getBishopAttacks(sq1, 0), sq2))
                lineThruBb[sq1][sq2] |= getBishopAttacks(sq1, 0) & getBishopAttacks(sq2, 0);
            else if (hasSquare(getRookAttacks(sq1, 0), sq2))
                lineThruBb[sq1][sq2] |= getRookAttacks(sq1, 0) & getRookAttacks(sq2, 0);

            if (lineThruBb[sq1][sq2] > 0)
                lineThruBb[sq1][sq2] |= squareBb(sq1) | squareBb(sq2);
        }
    }

    return lineThruBb;
}();

// Test pawn attacks
static_assert(lsb(getPawnAttacks(Square::A2, Color::White)) == Square::B3);
static_assert(getPawnAttacks(Square::B7, Color::White) == 0x500000000000000ULL);
static_assert(getPawnAttacks(Square::B7, Color::Black) == 0x50000000000ULL);
static_assert(lsb(getPawnAttacks(Square::H4, Color::Black)) == Square::G3);

// Test knight attacks
static_assert(getKnightAttacks(Square::A1) == 0x20400ULL);
static_assert(getKnightAttacks(Square::D4) == 0x142200221400ULL);
static_assert(getKnightAttacks(Square::G6) == 0xa0100010a0000000ULL);

// Test bishop attacks
static_assert(getBishopAttacks(Square::A1, 0) == 0x8040201008040200ULL);
static_assert(getBishopAttacks(Square::B2, 0x1000010000ULL) == 0x1008050005ULL);
static_assert(getBishopAttacks(Square::E4, 0x2800280000ULL) == 0x2800280000ULL);

// Test rook attacks
static_assert(getRookAttacks(Square::B1, 0) == 0x2020202020202fdULL);
static_assert(getRookAttacks(Square::B1, squareBb(Square::H1)) == 0x2020202020202fdULL);
static_assert(getRookAttacks(Square::B1, squareBb(Square::E1)) == 0x20202020202021dULL);

// Test queen attacks
static_assert(getQueenAttacks(Square::D4, 0x8000000002000000ULL) == 0x88492a1cf61c2a49ULL);

// Test king attacks
static_assert(getKingAttacks(Square::A1) == 0x302ULL);
static_assert(getKingAttacks(Square::D4) == 0x1c141c0000ULL);

// Test BETWEEN_EXCLUSIVE_BB
static_assert(BETWEEN_EXCLUSIVE_BB[Square::A1][Square::A1] == 0);
static_assert(BETWEEN_EXCLUSIVE_BB[Square::B2][Square::B2] == 0);
static_assert(BETWEEN_EXCLUSIVE_BB[Square::B2][Square::C4] == 0);
static_assert(BETWEEN_EXCLUSIVE_BB[Square::B2][Square::C3] == 0);
static_assert(lsb(BETWEEN_EXCLUSIVE_BB[Square::B2][Square::D4]) == Square::C3);
static_assert(BETWEEN_EXCLUSIVE_BB[Square::A5][Square::D8] == 0x4020000000000ULL);

// Test LINE_THRU_BB
static_assert(LINE_THRU_BB[Square::A1][Square::A1] == 0);
static_assert(LINE_THRU_BB[Square::B2][Square::B2] == 0);
static_assert(LINE_THRU_BB[Square::B2][Square::C4] == 0);
static_assert(LINE_THRU_BB[Square::B2][Square::C3] == 0x8040201008040201ULL);
static_assert(LINE_THRU_BB[Square::B3][Square::G8] == 0x4020100804020100ULL);
