#ifndef MOVE_HPP
#define MOVE_HPP

// clang-format off
#include "types.hpp"
#include "builtin.hpp"
#include "utils.hpp"

const NULL_FLAG = 0b0000,
      NORMAL_FLAG = 0b0001,
      CASTLE_FLAG = 0b0010,
      PROMOTION_FLAG = 0b0011;

inline uint16_t createMove(uint8_t from, uint8_t to, PieceType pieceType)
{
    // 16 bits: ffffff tttttt xxxx     
    uint16_t move = 0;          
    move = (from << 2) & (to >> 6);
    if (pieceType == PieceType::PAWN)
    {
        uint8_t rank = squareRank(to);
        if (rank == 0 || rank == 7) move 
    }

    return move;

};

#endif