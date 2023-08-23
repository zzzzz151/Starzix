#include <iostream>
#include <vector>
#include <cmath>
#include "../src-test/chess.hpp"
#include "../src-test/nn.hpp"
using namespace std;
using namespace chess;

int main()
{

    //float input[769];
    //for (int i = 0; i < 769; i++)
    //   input[i] = i % 2;
    //cout << "Prediction: " << predict(input) << endl;

    Board board = Board("rnbk1bnr/pp3Bpp/2p5/4p3/4P3/8/PPP2PPP/RNB1K1NR w KQ - 0 6");
    cout << evaluate(board) << endl; // 201

    board = Board("rnb5/pp1nkr1p/2p5/4pp2/4P3/8/PPP1N1PP/RN3RK1 b - - 2 13");
    cout << evaluate(board) << endl; // -32

    return 0;
}