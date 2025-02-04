// clang-format off

#include "types.hpp"

#pragma once

constexpr i32 MAX_DEPTH = 100;
constexpr i32 INF = 30'000;
constexpr i32 MIN_MATE_SCORE = INF - MAX_DEPTH;

// Move overhead milliseconds
constexpr u32 OVERHEAD_MS = 20;

// Hard time limit is divided by this, so this must not be 0
constexpr u32 DEFAULT_MOVES_TO_GO = 25;
