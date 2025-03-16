// clang-format off

#pragma once

#include "types.hpp"
#include "array_utils.hpp"
#include "enum_utils.hpp"
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <sstream>
#include <chrono>
#include <cassert>
#include <bit>
#include <bitset>

// General utils

#define stringify(myVar) static_cast<std::string>(#myVar)

#define ln(x) std::log(x)

template <typename T>
inline std::string colored(const T& x, const ColorCode colorCode)
{
    const std::string prefix = "\033[" + std::to_string(static_cast<i32>(colorCode)) + "m";
    const std::string suffix = "\033[0m";

    if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, std::string>)
    {
        return prefix + static_cast<std::string>(x) + suffix;
    }
    else {
        return prefix + std::to_string(x) + suffix;
    }
}

inline void trim(std::string& str)
{
    const size_t first = str.find_first_not_of(" \t\n\r");
    const size_t last  = str.find_last_not_of(" \t\n\r");

    str = first == std::string::npos
        ? ""
        : str.substr(first, last - first + 1);
}

inline std::vector<std::string> splitString(const std::string& str, const char delimiter)
{
    std::vector<std::string> strSplit = { };
    std::stringstream ss(str);
    std::string token;

    while (getline(ss, token, delimiter))
    {
        trim(token);

        if (token != "")
            strSplit.push_back(token);
    }

    return strSplit;
}

constexpr i32 charToI32(const char myChar)
{
    return myChar - '0';
}

inline u64 millisecondsElapsed(const std::chrono::steady_clock::time_point start)
{
    const auto now = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

    assert(duration.count() >= 0);
    return static_cast<u64>(duration.count());
}

constexpr u64 getNps(const u64 nodes, const u64 msElapsed)
{
    return nodes * 1000 / std::max<u64>(msElapsed, 1);
}

constexpr u64 nextU64(u64& state)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717ULL;
}

constexpr u64 pdep(const u64 val, u64 mask)
{
    u64 res = 0;

    for (u64 bb = 1; mask; bb += bb)
    {
        if (val & bb)
            res |= mask & -mask;

        mask &= mask - 1;
    }

    return res;
}

// Square utils

constexpr Square toSquare(const File f, const Rank r)
{
    return asEnum<Square>(static_cast<i32>(f) + static_cast<i32>(r) * 8);
}

constexpr File squareFile(const Square square)
{
    return asEnum<File>(static_cast<i32>(square) % 8);
}

constexpr Rank squareRank(const Square square)
{
    return asEnum<Rank>(static_cast<i32>(square) / 8);
}

inline Square strToSquare(const std::string strSquare)
{
    assert(strSquare.size() == 2);

    const char fileChar = static_cast<char>(std::tolower(strSquare[0]));
    const char rankChar = strSquare[1];

    const File file = asEnum<File>(fileChar - 'a');
    const Rank rank = asEnum<Rank>(charToI32(rankChar) - 1);

    return toSquare(file, rank);
}

inline std::string squareToStr(const Square square)
{
    const char file = 'a' + static_cast<char>(squareFile(square));
    const char rank = '1' + static_cast<char>(squareRank(square));

    return std::string{file, rank};
}

constexpr Square flipFile(const Square square)
{
    return asEnum<Square>(static_cast<i32>(square) ^ 7);
}

constexpr Square flipRank(const Square square)
{
    return asEnum<Square>(static_cast<i32>(square) ^ 56);
}

constexpr Square enPassantRelative(const Square square)
{
    return asEnum<Square>(static_cast<i32>(square) ^ 8);
}

// Piece type utils

constexpr char letterOf(const Color color, const PieceType pieceType)
{
    constexpr auto PIECE_TYPE_TO_LETTER = EnumArray<char, PieceType> {
        'P', 'N', 'B', 'R', 'Q', 'K'
    };

    const char letter = PIECE_TYPE_TO_LETTER[pieceType];

    return color == Color::White ? letter : static_cast<char>(std::tolower(letter));
}

// File and rank utils

constexpr bool isBackrank(const Rank rank) {
    return rank == Rank::Rank1 || rank == Rank::Rank8;
}

// Bitboard utils

constexpr Bitboard squareBb(const Square square)
{
    return 1ULL << static_cast<i32>(square);
}

constexpr bool hasSquare(const Bitboard bb, const Square square)
{
    return (bb & squareBb(square)) > 0;
}

constexpr Bitboard fileBb(const File file)
{
    constexpr Bitboard FILE_A_BB = 0x101010101010101ULL;
    return FILE_A_BB << static_cast<i32>(file);
}

constexpr Bitboard rankBb(const Rank rank)
{
    constexpr Bitboard RANK_1_BB = 0xff;
    return RANK_1_BB << (static_cast<i32>(rank) * 8);
}

constexpr Square lsb(const Bitboard bb)
{
    assert(bb > 0);
    return asEnum<Square>(std::countr_zero(bb));
}

constexpr auto popLsb(Bitboard& bb)
{
    const auto square = lsb(bb);
    bb &= bb - 1; // Compiler optimizes this to _blsr_u64
    return square;
}

#define ITERATE_BITBOARD(bitboard, square, expr) \
    { \
        Bitboard tempBb = (bitboard); \
        while (tempBb > 0) { \
            const Square square = popLsb(tempBb); \
            expr; \
        } \
    }

inline void printBitboard(const Bitboard bb)
{
    const std::bitset<64> b(bb);
    const std::string strBitset = b.to_string();

    for (size_t i = 0; i < 64; i += 8)
    {
        std::string x = strBitset.substr(i, 8);
        reverse(x.begin(), x.end());

        for (size_t j = 0; j < x.length(); j++)
            std::cout << std::string(1, x[j]) << " ";

        std::cout << std::endl; // Flush
    }
}

// Zobrist hashing

constexpr u64 ZOBRIST_COLOR = 591679071752537765ULL;

// [color][pieceType][square]
constexpr EnumArray<u64, Color, PieceType, Square> ZOBRIST_PIECES = [] () consteval
{
    EnumArray<u64, Color, PieceType, Square> zobristPieces = { };

    u64 rngState = 35907035218217ULL;

    for (const Color color : EnumIter<Color>())
        for (const PieceType pieceType : EnumIter<PieceType>())
            for (const Square square : EnumIter<Square>())
                zobristPieces[color][pieceType][square] = nextU64(rngState);

    return zobristPieces;
}();

// [file]
constexpr EnumArray<u64, File> ZOBRIST_FILES = {
    12228382040141709029ULL, 2494223668561036951ULL,
    7849557628814744642ULL, 16000570245257669890ULL,
    16614404541835922253ULL, 17787301719840479309ULL,
    6371708097697762807ULL, 7487338029351702425ULL,
};

static_assert(squareFile(Square::A1) == File::A);
static_assert(squareFile(Square::C4) == File::C);
static_assert(squareFile(Square::H8) == File::H);

static_assert(squareRank(Square::A1) == Rank::Rank1);
static_assert(squareRank(Square::C4) == Rank::Rank4);
static_assert(squareRank(Square::H8) == Rank::Rank8);

static_assert(fileBb(File::A) == 0x101010101010101ULL);
static_assert(fileBb(File::D) == 0x808080808080808ULL);
static_assert(fileBb(File::H) == 0x8080808080808080ULL);

static_assert(rankBb(Rank::Rank1) == 0xffULL);
static_assert(rankBb(Rank::Rank4) == 0xff000000ULL);
static_assert(rankBb(Rank::Rank8) == 0xff00000000000000ULL);

static_assert(lsb(0x40010000000ULL) == Square::E4);
static_assert(lsb(0x8000000000000000ULL) == Square::H8);
