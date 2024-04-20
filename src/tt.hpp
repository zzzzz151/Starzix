// clang-format off

#pragma once

enum class Bound {
    INVALID = 0,
    EXACT = 1,
    LOWER = 2,
    UPPER = 3
};

struct TTEntry {
    public:
    u64 zobristHash = 0;
    i16 score = 0;
    Move bestMove = MOVE_NONE;
    u8 depth = 0; 
    u8 boundAndAge = 0; // lowest 2 bits for bound, highest 6 bits for age

    inline i16 adjustedScore(i16 ply)
    {
        if (score >= MIN_MATE_SCORE)
            return score - ply;
        if (score <= -MIN_MATE_SCORE)
            return score + ply;
        return score;
    }

    inline Bound getBound() {
         return (Bound)(boundAndAge & 0b0000'0011); 
    }

    inline void setBound(Bound newBound) { 
        boundAndAge = (boundAndAge & 0b1111'1100) | (u8)newBound; 
    }

    inline u8 getAge() { 
        return boundAndAge >> 2;
    }

    inline void setAge(u8 newAge) { 
        assert(newAge <= 63);
        boundAndAge &= 0b0000'0011;
        boundAndAge |= (newAge << 2);
    }

    inline void setBoundAndAge(Bound bound, u8 age) {
        assert(age <= 63);
        boundAndAge = (age << 2) | (u8)bound;
    }

    inline void update(u64 zobristHash, u8 depth, u8 ply, i16 score, Move bestMove, Bound bound)
    {
        this->zobristHash = zobristHash;
        this->depth = depth;
        this->setBound(bound);
        this->score = score >= MIN_MATE_SCORE 
                      ? score + (i16)ply 
                      : score <= -MIN_MATE_SCORE 
                      ? score - (i16)ply 
                      : score;

        if (bestMove != MOVE_NONE && (this->bestMove == MOVE_NONE || bound != Bound::UPPER))
            this->bestMove = bestMove;
    }

    inline void update(u64 zobristHash, u8 depth, u8 ply,  i16 score, 
                       Move bestMove, i16 originalAlpha, i16 beta)
    {
        Bound bound = score <= originalAlpha ? Bound::UPPER 
                      : score >= beta ? Bound::LOWER 
                      : Bound::EXACT;

        update(zobristHash, depth, ply, score, bestMove, bound);
    }

} __attribute__((packed)); // struct TTEntry

inline TTEntry* probeTT(std::vector<TTEntry> &tt, u64 zobristHash) 
{
    u64 idx = ((u128)zobristHash * (u128)tt.size()) >> 64;
    return &tt[idx];
}