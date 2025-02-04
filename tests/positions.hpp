// clang-format off

#pragma once

#include "../src/position.hpp"

const std::string FEN_START          = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const std::string FEN_KIWIPETE       = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
const std::string FEN_POS_3          = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
const std::string FEN_POS_4          = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
const std::string FEN_POS_4_MIRRORED = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
const std::string FEN_POS_5          = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";

const Position POS_KIWIPETE   = Position(FEN_KIWIPETE);
const Position POS_3          = Position(FEN_POS_3);
const Position POS_4          = Position(FEN_POS_4);
const Position POS_4_MIRRORED = Position(FEN_POS_4_MIRRORED);
const Position POS_5          = Position(FEN_POS_5);

const Position POS_IN_CHECK  = Position("k7/8/K1Q5/8/8/8/8/8 b - - 0 1");
const Position POS_CHECKMATE = Position("k1Q5/7n/K7/8/8/8/8/8 b - - 0 1");
const Position POS_STALEMATE = Position("k7/8/KQ6/8/8/8/8/8 b - - 0 1");
