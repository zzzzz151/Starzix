// clang-format off
#include "../src/board.hpp"
#include "../src/nnue.hpp"

std::unordered_map<std::string, int> FENS_EVAL = {
    {START_FEN, 109},
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", -64},  // e2e4
    {"rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 119}, // e2e4 g8f6

    // Moves in "r3k2b/6P1/8/1pP5/8/8/4P3/4K2R w Kq b6 0 1"
    {"r3k2b/6P1/1P6/8/8/8/4P3/4K2R b Kq - 0 1", -99},
    {"r3k2b/6P1/8/1pP5/8/4P3/8/4K2R b Kq - 0 1", 286},
    {"r3k2b/6P1/8/1pP5/4P3/8/8/4K2R b Kq e3 0 1", 305},
    {"r3k2b/6P1/2P5/1p6/8/8/4P3/4K2R b Kq - 0 1", 161},
    {"r3k2Q/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -2436},
    {"r3k2R/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1807},
    {"r3k2B/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1067},
    {"r3k2N/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -757},
    {"r3k1Qb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -1617},
    {"r3k1Rb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -969},
    {"r3k1Bb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", -93},
    {"r3k1Nb/8/8/1pP5/8/8/4P3/4K2R b Kq - 0 1", 109},
    {"r3k2b/6P1/8/1pP5/8/8/4P3/3K3R b q - 1 1", 242},
    {"r3k2b/6P1/8/1pP5/8/8/4P3/5K1R b q - 1 1", 305},
    {"r3k2b/6P1/8/1pP5/8/8/3KP3/7R b q - 1 1", 155},
    {"r3k2b/6P1/8/1pP5/8/8/4PK2/7R b q - 1 1", 236},
    {"r3k2b/6P1/8/1pP5/8/8/4P3/5RK1 b q - 1 1", 289},
    {"r3k2b/6P1/8/1pP5/8/8/4P3/4KR2 b q - 1 1", 212},
    {"r3k2b/6P1/8/1pP5/8/8/4P3/4K1R1 b q - 1 1", 219},
    {"r3k2b/6P1/8/1pP5/8/8/4P2R/4K3 b q - 1 1", 269},
    {"r3k2b/6P1/8/1pP5/8/7R/4P3/4K3 b q - 1 1", 208},
    {"r3k2b/6P1/8/1pP5/7R/8/4P3/4K3 b q - 1 1", 200},
    {"r3k2b/6P1/8/1pP4R/8/8/4P3/4K3 b q - 1 1", 183},
    {"r3k2b/6P1/7R/1pP5/8/8/4P3/4K3 b q - 1 1", 203},
    {"r3k2b/6PR/8/1pP5/8/8/4P3/4K3 b q - 1 1", 158},
    {"r3k2R/6P1/8/1pP5/8/8/4P3/4K3 b q - 0 1", -1233}
};

void testAccumulatorUpdate(Board board, Move move) 
{
    Accumulator acc = Accumulator(board);
    Accumulator oldAcc = acc;
    
    board.makeMove(move);
    std::string fen = board.fen();

    if (FENS_EVAL.find(fen) == FENS_EVAL.end())
    {
        std::cout << "No fen '" << fen << "'" << std::endl;
        return;
    }
    
    acc.updated = false;
    acc.update(&oldAcc, board);

    auto eval = evaluate(&acc, board, false);

    if (eval != FENS_EVAL[fen]) 
        std::cout << "Expected eval " << FENS_EVAL[fen] 
                  << " but got " << eval 
                  << " in '" << fen << "'" 
                  << std::endl;
}

int main()
{
    attacks::init();
    initUtils();
    initZobrist();

    std::cout << "Testing Accumulator(Board) ..." << std::endl;

    for (const auto& [fen, expectedEval] : FENS_EVAL) 
    {
        Board board = Board(fen);
        Accumulator acc = Accumulator(board);
        auto eval = evaluate(&acc, board, false);

        if (eval != expectedEval) 
            std::cout << "Expected eval " << expectedEval
                      << " but got " << eval 
                      << " in '" << fen << "'" 
                      << std::endl;
    }

    std::cout << "Testing Accumulator.update() ..." << std::endl;

    Board board = Board(START_FEN);
    testAccumulatorUpdate(board, board.uciToMove("e2e4"));

    board.makeMove(board.uciToMove("e2e4"));
    testAccumulatorUpdate(board, board.uciToMove("g8f6"));

    board = Board("r3k2b/6P1/8/1pP5/8/8/4P3/4K2R w Kq b6 0 1");
    u64 pinned = board.pinned();

    MovesList moves;
    board.pseudolegalMoves(moves);

    for (int i = 0; i < moves.size(); i++) 
        if (board.isPseudolegalLegal(moves[i], pinned)) 
            testAccumulatorUpdate(board, moves[i]);

    return 0;
}
