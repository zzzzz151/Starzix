#include "chess.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <unordered_map>
#include <cstring>

using namespace std;
using namespace chess;

Move NULL_MOVE;
const int POS_INFINITY = 9999999, NEG_INFINITY = -9999999;
const PieceType PIECE_TYPES[7] = {PieceType::NONE, PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN, PieceType::KING};
const int PIECE_VALUES[7] = {0, 100, 302, 320, 500, 900, 15000};
unordered_map<PieceType, int> PIECE_TYPE_TO_VALUE = {
    {PieceType::NONE, PIECE_VALUES[0]},
    {PieceType::PAWN, PIECE_VALUES[1]},
    {PieceType::KNIGHT, PIECE_VALUES[2]},
    {PieceType::BISHOP, PIECE_VALUES[3]},
    {PieceType::ROOK, PIECE_VALUES[4]},
    {PieceType::QUEEN, PIECE_VALUES[5]},
    {PieceType::KING, PIECE_VALUES[6]}};

const char EXACT = 1, LOWER_BOUND = 2, UPPER_BOUND = 3;
struct TTEntry
{
    U64 key;
    int score;
    Move bestMove;
    int depth;
    char type;
};
TTEntry TT[0x800000];

int PIECE_PHASE[] = {0, 0, 1, 1, 2, 4, 0};
U64 PSTs[] = {657614902731556116, 420894446315227099, 384592972471695068, 312245244820264086, 364876803783607569, 366006824779723922, 366006826859316500, 786039115310605588, 421220596516513823, 366011295806342421, 366006826859316436, 366006896669578452, 162218943720801556, 440575073001255824, 657087419459913430, 402634039558223453, 347425219986941203, 365698755348489557, 311382605788951956, 147850316371514514, 329107007234708689, 402598430990222677, 402611905376114006, 329415149680141460, 257053881053295759, 291134268204721362, 492947507967247313, 367159395376767958, 384021229732455700, 384307098409076181, 402035762391246293, 328847661003244824, 365712019230110867, 366002427738801364, 384307168185238804, 347996828560606484, 329692156834174227, 365439338182165780, 386018218798040211, 456959123538409047, 347157285952386452, 365711880701965780, 365997890021704981, 221896035722130452, 384289231362147538, 384307167128540502, 366006826859320596, 366006826876093716, 366002360093332756, 366006824694793492, 347992428333053139, 457508666683233428, 329723156783776785, 329401687190893908, 366002356855326100, 366288301819245844, 329978030930875600, 420621693221156179, 422042614449657239, 384602117564867863, 419505151144195476, 366274972473194070, 329406075454444949, 275354286769374224, 366855645423297932, 329991151972070674, 311105941360174354, 256772197720318995, 365993560693875923, 258219435335676691, 383730812414424149, 384601907111998612, 401758895947998613, 420612834953622999, 402607438610388375, 329978099633296596, 67159620133902};

Board board;
Move killerMoves[512][2];
Move bestMoveRoot;
float timeForThisTurn;
chrono::steady_clock::time_point start;

bool isTimeUp()
{
    return (chrono::steady_clock::now() - start) / std::chrono::milliseconds(1) >= timeForThisTurn;
}

int getPSTValue(int psq)
{
    return (int)(((PSTs[psq / 10] >> (6 * (psq % 10))) & 63) - 20) * 8;
}

int evaluate()
{
    int mg = 0, eg = 0, phase = 0;

    for (bool stm : {true, false})
    {
        for (int i = 1; i <= 6; i++)
        {
            int ind;
            Bitboard mask = board.pieces(PIECE_TYPES[i], stm ? Color::WHITE : Color::BLACK);
            while (mask != 0)
            {
                phase += PIECE_PHASE[i];
                ind = 128 * (i - 1) + builtin::poplsb(mask) ^ (stm ? 56 : 0);
                mg += getPSTValue(ind) + PIECE_VALUES[i];
                eg += getPSTValue(ind + 64) + PIECE_VALUES[i];
            }
        }

        mg = -mg;
        eg = -eg;
    }

    return (mg * phase + eg * (24 - phase)) / 24 * (board.sideToMove() == Color::WHITE ? 1 : -1);
}

/*
int SEE(Square square)
{
    // cout << "SEE beginning" << endl;

    int value = 0;
    PieceType mySmallestAttacker = PieceType::NONE;

    Bitboard occ = board.occ();
    Bitboard attacksAtSquare[] = {movegen::attacks::pawn(board.sideToMove() == Color::WHITE ? Color::BLACK : Color::WHITE, square),
                                  movegen::attacks::knight(square),
                                  movegen::attacks::bishop(square, occ),
                                  movegen::attacks::rook(square, occ),
                                  movegen::attacks::queen(square, occ),
                                  movegen::attacks::king(square)};
    int bitIndex;
    for (int i = 1; i <= 6; i++)
    {
        Bitboard myPieces = board.pieces(PIECE_TYPES[i], board.sideToMove());
        Bitboard masked = myPieces & attacksAtSquare[i - 1];
        if (masked > 0)
        {
            mySmallestAttacker = PIECE_TYPES[i];
            bitIndex = builtin::lsb(masked);
            break;
        }
    }

    // skip if the square isn't attacked anymore by this side
    if (mySmallestAttacker != PieceType::NONE)
    {
        Square from = (Square)bitIndex;
        Move move = Move::make<Move::NORMAL>(from, square);
        int valueOfPieceCaptured = PIECE_TYPE_TO_VALUE[board.at<PieceType>(square)];

        board.makeMove(move);
        value = valueOfPieceCaptured - SEE(square);
        // Do not consider captures if they lose material, therefor max zero
        if (value < 0)
            value = 0;
        board.unmakeMove(move);
    }

    // cout << "SEE end" << endl;

    return value;
}

int SEECapture(Move capture)
{
    // cout << "SEECapture beginning " << uci::moveToUci(capture) << endl;

    int value = 0;
    int valueOfPieceCaptured = PIECE_TYPE_TO_VALUE[board.at<PieceType>(capture.to())];
    board.makeMove(capture);
    value = valueOfPieceCaptured - SEE(capture.to());
    board.unmakeMove(capture);
    // cout << "SEECapture end " << uci::moveToUci(capture) << " " << value << endl;
    return value;
}
*/

void orderMoves(U64 boardKey, U64 indexInTT, Movelist &moves, int plyFromRoot)
{
    for (int i = 0; i < moves.size(); i++)
    {
        if (TT[indexInTT].key == boardKey && moves[i] == TT[indexInTT].bestMove)
            moves[i].setScore(32000);
        else if (board.isCapture(moves[i]))
        {

            PieceType captured = board.at<PieceType>(moves[i].to());
            PieceType capturing = board.at<PieceType>(moves[i].from());
            int moveScore = 30 * PIECE_TYPE_TO_VALUE[captured] - PIECE_TYPE_TO_VALUE[capturing];
            moves[i].setScore(moveScore);
            // moves[i].setScore(SEECapture(moves[i]));
        }
        else if (moves[i].typeOf() == moves[i].PROMOTION)
            moves[i].setScore(-31980);
        else if (killerMoves[plyFromRoot][0] == moves[i] || killerMoves[plyFromRoot][1] == moves[i])
            moves[i].setScore(-31990);
        else
            moves[i].setScore(-32000);
    }
    moves.sort();
}

int qSearch(int alpha, int beta, int plyFromRoot)
{
    // Quiescence saarch: search capture moves until a 'quiet' position is reached

    int eval = evaluate();
    if (eval >= beta)
        return beta;

    if (alpha < eval)
        alpha = eval;

    Movelist captures;
    movegen::legalmoves<MoveGenType::CAPTURE>(captures, board);
    U64 boardKey = board.zobrist();
    U64 indexInTT = 0x7FFFFF & boardKey;
    orderMoves(boardKey, indexInTT, captures, plyFromRoot);

    for (const auto &capture : captures)
    {
        board.makeMove(capture);
        int score = -qSearch(-beta, -alpha, plyFromRoot + 1);
        board.unmakeMove(capture);

        if (score >= beta)
            return score; // in CPW: return beta;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

void savePotentialKillerMove(Move move, int plyFromRoot)
{
    if (!board.isCapture(move) && move != killerMoves[plyFromRoot][0])
    {
        killerMoves[plyFromRoot][1] = killerMoves[plyFromRoot][0];
        killerMoves[plyFromRoot][0] = move;
    }
}

int negamax(int depth, int plyFromRoot, int alpha, int beta, bool doNull = true)
{
    if (plyFromRoot > 0 && board.isRepetition())
        return 0;

    int originalAlpha = alpha;
    bool inCheck = board.inCheck();
    if (inCheck)
        depth++;

    U64 boardKey = board.zobrist();
    U64 indexInTT = 0x7FFFFF & boardKey;
    if (plyFromRoot > 0 && TT[indexInTT].key == boardKey && TT[indexInTT].depth >= depth)
        if (TT[indexInTT].type == EXACT || (TT[indexInTT].type == LOWER_BOUND && TT[indexInTT].score >= beta) || (TT[indexInTT].type == UPPER_BOUND && TT[indexInTT].score <= alpha))
            return TT[indexInTT].score;

    // Internal iterative reduction
    if (TT[indexInTT].key != boardKey && depth > 3)
        depth--;

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty())
        return inCheck ? NEG_INFINITY + plyFromRoot : 0; // else it's draw

    if (depth <= 0)
        return qSearch(alpha, beta, plyFromRoot);

    // Reverse futility pruning
    int eval = evaluate();
    if (!inCheck && depth <= 7 && eval >= beta + 80 * depth / 1.5f)
        return eval;

    // Null move pruning
    if (!inCheck && plyFromRoot > 0 && doNull && depth >= 3)
    {
        bool hasAtLeast1Piece = board.pieces(PieceType::KNIGHT, board.sideToMove()) > 0 || board.pieces(PieceType::BISHOP, board.sideToMove()) > 0 || board.pieces(PieceType::ROOK, board.sideToMove()) > 0 || board.pieces(PieceType::QUEEN, board.sideToMove()) > 0;
        if (hasAtLeast1Piece)
        {
            board.makeNullMove();
            int eval = -negamax(depth - 3, plyFromRoot + 1, -beta, -alpha, false);
            board.unmakeNullMove();
            if (eval >= beta)
                return beta;
            if (isTimeUp())
                return POS_INFINITY;
        }
    }

    orderMoves(boardKey, indexInTT, moves, plyFromRoot);
    int bestEval = NEG_INFINITY;
    Move bestMove;
    bool pv = plyFromRoot == 0 || beta - alpha > 1;
    for (int i = 0; i < moves.size(); i++)
    {
        Move move = moves[i];
        board.makeMove(move);
        bool tactical = board.isCapture(move) || move.promotionType() == PieceType::QUEEN || board.inCheck();
        bool shouldLmr = i >= 3 && !tactical && !inCheck;
        int eval = 2147483647;
        if (shouldLmr)
            eval = -negamax(depth - 2, plyFromRoot + 1, -beta, -alpha);
        if (eval > alpha)
            eval = -negamax(depth - 1, plyFromRoot + 1, -beta, -alpha);
        /*
        if (i==0)
            eval = -negamax(depth - 1, plyFromRoot + 1, -beta, -alpha);
        else
        {
            bool tactical = board.isCapture(move) || move.promotionType() == PieceType::QUEEN || board.inCheck();
            bool doLmr = i >= 3 && !tactical && !inCheck;
            eval = -negamax(depth - 1 - doLmr, plyFromRoot + 1, -alpha-1, -alpha);
            if (eval > alpha && (pv || doLmr)) // bool pv = plyFromRoot == 0 || beta - alpha > 1;
                eval = -negamax(depth - 1, plyFromRoot + 1, -beta, -alpha);
        }
        */
        board.unmakeMove(move);

        if (eval > bestEval)
        {
            bestEval = eval;
            bestMove = move;
            if (plyFromRoot == 0)
                bestMoveRoot = move;

            if (bestEval > alpha)
                alpha = bestEval;
            if (alpha >= beta)
            {
                savePotentialKillerMove(move, plyFromRoot);
                break;
            }
        }

        if (isTimeUp())
            return POS_INFINITY;
    }

    TT[indexInTT].key = boardKey;
    TT[indexInTT].depth = depth;
    TT[indexInTT].score = bestEval;
    TT[indexInTT].bestMove = bestMove;
    if (bestEval <= originalAlpha)
        TT[indexInTT].type = UPPER_BOUND;
    else if (bestEval >= beta)
        TT[indexInTT].type = LOWER_BOUND;
    else
        TT[indexInTT].type = EXACT;

    return bestEval;
}

Move bestMoveRootAsp;
int delta = 15;
int score;
void aspiration(int maxDepth)
{
    int alpha = max(NEG_INFINITY, score - delta);
    int beta = min(POS_INFINITY, score + delta);
    int depth = maxDepth;

    while (!isTimeUp())
    {
        score = negamax(depth, 0, alpha, beta);
        if (isTimeUp())
            break;

        if (score >= beta)
        {
            beta = min(beta + delta, POS_INFINITY);
            bestMoveRootAsp = bestMoveRoot;
            depth--;
        }
        else if (score <= alpha)
        {
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, NEG_INFINITY);
            depth = maxDepth;
        }
        else
            break;
            

        delta *= 2;
    }
}

void iterativeDeepening(float millisecondsLeft)
{
    start = chrono::steady_clock::now();
    timeForThisTurn = millisecondsLeft / (float)30;
    bestMoveRootAsp = NULL_MOVE;

    for (int iterationDepth = 1; !isTimeUp(); iterationDepth++)
    {
        if (iterationDepth < 6)
            score = negamax(iterationDepth, 0, NEG_INFINITY, POS_INFINITY);
        else
            aspiration(iterationDepth);
    }
}

void position(vector<string> words)
{
    int movesTokenIndex = -1;
    if (words[1] == "startpos")
    {
        board = Board(STARTPOS);
        movesTokenIndex = 2;
    }
    else if (words[1] == "fen")
    {
        string fen = "";
        for (int i = 2; i < 8; i++)
            fen += words[i];
        board = Board(fen);
        movesTokenIndex = 8;
    }

    for (int i = movesTokenIndex + 1; i < words.size(); i++)
        board.makeMove(uci::uciToMove(board, words[i]));
}

void uciLoop()
{
    string received;
    getline(cin, received);
    cout << "id name test\n";
    cout << "id author zzzzz\n";
    cout << "uciok\n";

    while (true)
    {
        getline(cin, received);
        istringstream stringStream(received);
        vector<string> words;
        string word;
        while (getline(stringStream, word, ' '))
            words.push_back(word);

        if (received == "quit" || !cin.good())
            break;
        else if (received == "ucinewgame")
            memset(TT, 0, sizeof(TT)); // Clear TT
        else if (received == "isready")
            cout << "readyok\n";
        else if (words[0] == "position")
        {
            cout << "new pos" << endl;
            memset(killerMoves, 0, sizeof(killerMoves));
            position(words);
        }
        else if (words[0] == "go")
        {
            int millisecondsLeft = board.sideToMove() == Color::WHITE ? stoi(words[2]) : stoi(words[4]);
            iterativeDeepening(millisecondsLeft);
            cout << "bestmove " + uci::moveToUci(bestMoveRootAsp == NULL_MOVE ? bestMoveRoot : bestMoveRootAsp) + "\n";
        }
    }
}

int main()
{
    uciLoop();
    return 0;
}
