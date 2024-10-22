// clang-format-off

#pragma once

#include "board.hpp"
#include "search.hpp"
#include "bench.hpp"
#include <thread>

class UciEngine {
    private:

    SearchThread* mMainSearchThread;
    std::deque<SearchThread*> mSearchThreads = { };
    std::deque<std::thread>   mNativeThreads = { };

    SearchSettings mSearchSettings = { };

    std::vector<TTEntry> mTT = { };

    public:

    Engine() {
        resizeTT(mTT, 32);
        printTTSize(mTT);

        mMainSearchThread = new SearchThread(&mTT);
    }

    ~Engine() {
        while (!mSearchThreads.empty())
            removeSecondaryThread();

        assert(mNativeThreads.empty());
    }

    inline void wakeThreads(const ThreadAction action)
    {
        mMainSearchThread->wake(action);

        for (SearchThread &searchThread : mSearchThreads)
            searchThread->wake(action);
    }

    inline void addSecondaryThread() {
        SearchThread* searchThread = new SearchThread(&mTT);
        std::thread nativeThread([=]() mutable { searchThread->loop(); });

        mSearchThreads.push_back(searchThread);
        mNativeThreads.push_back(std::move(nativeThread));
    }

    inline void removeSecondaryThread() {
        assert(mSearchThreads.size() == mNativeThreads.size());

        if (mSearchThreads.size() == 0) return;

        mSearchThreads.back()->signalAndAwaitShutdown();

        if (mNativeThreads.back().joinable())
            mNativeThreads.back().join();

        mNativeThreads.pop_back();

        delete mSearchThreads.back();
        mSearchThreads.pop_back();
    }

    inline u64 totalNodes() {
        u64 nodes = mMainSearchThread.nodes();

        for (SearchThread* searchThread : mSearchThreads)
            nodes += searchThread->nodes();

        return nodes;
    }

    inline void uciLoop() 
    {
        std::string command = "";

        while (command != "quit") {
            std::getline(std::cin, command);
            runCommand(command);
        }
    }

    inline void runCommand(std::string &command)
    {
        trim(command);
        const std::vector<std::string> tokens = splitString(command, ' ');

        if (command == "" || tokens.size() == 0)
            return;
        // UCI commands
        else if (command == "uci")
            uci();
        else if (tokens[0] == "setoption") // e.g. "setoption name Hash value 32"
            setoption(tokens);
        else if (command == "ucinewgame")
            searchThread.reset();
        else if (command == "isready")
            std::cout << "readyok" << std::endl;
        else if (tokens[0] == "position")
            position(tokens);
        else if (tokens[0] == "go")
            go(tokens);
        // Non-UCI commands
        else if (command == "print" || command == "d"
        || command == "display" || command == "show")
            mMainSearchThread->mBoard.print();
        else if (tokens[0] == "bench")
        {
            if (tokens.size() == 1)
                bench();
            else {
                const int depth = stoi(tokens[1]);
                bench(depth);
            }
        }
        else if (command == "eval") 
        {
            const BothAccumulators acc = BothAccumulators(mMainSearchThread->mBoard);
            const i32 eval = nnue::evaluate(&acc, mMainSearchThread->mBoard.sideToMove());
            const i32 evalScaled = eval * materialScale(mMainSearchThread->mBoard);

            std::cout << "eval "    << eval
                      << " scaled " << evalScaled
                      << std::endl;
        }
        else if (tokens[0] == "perft" || (tokens[0] == "go" && tokens[1] == "perft"))
        {
            const int depth = stoi(tokens[1]);
            const std::string fen = mMainSearchThread->mBoard.fen();

            std::cout << "perft depth " << depth << " '" << fen << "'" << std::endl;

            const std::chrono::steady_clock::time_point start =  std::chrono::steady_clock::now();
            const u64 nodes = depth > 0 ? perft(mMainSearchThread->mBoard, depth) : 0;

            std::cout << "perft depth " << depth 
                      << " nodes " << nodes 
                      << " nps " << nodes * 1000 / std::max((u64)millisecondsElapsed(start), (u64)1)
                      << " time " << millisecondsElapsed(start)
                      << " fen " << fen
                      << std::endl;
        }
        else if (tokens[0] == "perftsplit" || tokens[0] == "splitperft" 
        || tokens[0] == "perftdivide" || tokens[0] == "divideperft")
        {
            const int depth = stoi(tokens[1]);
            
            std::cout << "perft split depth " << depth 
                    << " '" << mMainSearchThread->mBoard.fen() << "'"
                    << std::endl;

            if (depth <= 0) {
                std::cout << "Total: 0" << std::endl;
                return;
            }

            ArrayVec<Move, 256> moves;
            mMainSearchThread->mBoard.pseudolegalMoves(moves, MoveGenType::ALL);

            u64 totalNodes = 0;

            for (const Move move : moves) 
                if (mMainSearchThread->mBoard.isPseudolegalLegal(move))
                {
                    mMainSearchThread->mBoard.makeMove(move);
                    const u64 nodes = perft(mMainSearchThread->mBoard, depth - 1);
                    std::cout << move.toUci() << ": " << nodes << std::endl;
                    totalNodes += nodes;
                    mMainSearchThread->mBoard.undoMove();
                }

            std::cout << "Total: " << totalNodes << std::endl;
        }
        else if (tokens[0] == "makemove")
        {
            if ((tokens[1] == "0000" || tokens[1] == "null" || tokens[1] == "none")
            && !mMainSearchThread->mBoard.inCheck())
                mMainSearchThread->mBoard.makeMove(MOVE_NONE);
            else
                mMainSearchThread->mBoard.makeMove(tokens[1]);
        }
        else if (tokens[0] == "undomove")
            mMainSearchThread->mBoard.undoMove();
        #if defined(TUNE)
        else if (command == "spsainput") 
        {
            for (auto &pair : tunableParams) {
                std::string paramName = pair.first;
                auto &tunableParam = pair.second;

                std::visit([&paramName] (auto *myParam) 
                {
                    if (myParam == nullptr) return;

                    std::cout << paramName 
                            << ", " << (myParam->floatOrDouble() ? "float" : "int")
                            << ", " << myParam->value
                            << ", " << myParam->min
                            << ", " << myParam->max
                            << ", " << myParam->step
                            << ", 0.002"
                            << std::endl;
                }, tunableParam);
            }
        }
        #endif
    }

    inline void uci() {
        std::cout << "id name Starzix" << std::endl;
        std::cout << "id author zzzzz" << std::endl;
        std::cout << "option name Hash type spin default 32 min 1 max 65536" << std::endl;
        //std::cout << "option name Threads type spin default 1 min 1 max 256" << std::endl;

        #if defined(TUNE)
            for (auto &pair : tunableParams) {
                std::string paramName = pair.first;
                auto &tunableParam = pair.second;

                std::visit([&paramName] (auto *myParam) 
                {
                    if (myParam == nullptr) return;

                    std::cout << "option name " << paramName 
                            << " type string"
                            << " default " << myParam->value
                            << " min "     << myParam->min
                            << " max "     << myParam->max
                            << std::endl;
                    
                }, tunableParam);
            }
        #endif

        std::cout << "uciok" << std::endl;
    }

    inline void setoption(const std::vector<std::string> &tokens)
    {
        const std::string optionName  = tokens[2];
        const std::string optionValue = tokens[4];

        if (optionName == "Hash" || optionName == "hash")
        {
            resizeTT(mTT, stoll(optionValue));
            printTTSize(mTT);
        }
        else if (optionName == "Threads" || optionName == "threads")
        {
            const int numThreads = std::clamp(stoi(optionValue), 1, 256);
        }
        #if defined(TUNE)
        else if (tunableParams.count(optionName) > 0) 
        {
            auto tunableParam = tunableParams[optionName];

            std::visit([optionName, optionValue] (auto *myParam) 
            {
                if (myParam == nullptr) return;

                myParam->value = std::stod(optionValue);

                if (optionName == stringify(lmrBaseQuiet) || optionName == stringify(lmrBaseNoisy)
                || optionName == stringify(lmrMultiplierQuiet) || optionName == stringify(lmrMultiplierNoisy))
                    LMR_TABLE = getLmrTable();

                std::cout << optionName << " set to " << myParam->value << std::endl;
            }, tunableParam);
        }
        #endif
    }

    inline void position(const std::vector<std::string> &tokens)
    {
        int movesTokenIndex = -1;

        if (tokens[1] == "startpos") {
            mMainSearchThread->mBoard = Board(START_FEN);
            movesTokenIndex = 2;
        }
        else if (tokens[1] == "fen")
        {
            std::string fen = "";
            u64 i = 0;
            
            for (i = 2; i < tokens.size() && tokens[i] != "moves"; i++)
                fen += tokens[i] + " ";

            fen.pop_back(); // remove last whitespace
            mMainSearchThread->mBoard = Board(fen);
            movesTokenIndex = i;
        }

        for (u64 i = movesTokenIndex + 1; i < tokens.size(); i++)
            mMainSearchThread->mBoard.makeMove(tokens[i]);
    }

    inline void go(const std::vector<std::string> &tokens)
    {
        mSearchSettings = SearchSettings();

        u64 milliseconds = std::numeric_limits<u64>::max();
        [[maybe_unused]] u64 incrementMs = 0;
        [[maybe_unused]] u64 movesToGo = 0;
        bool isMoveTime = false;

        for (int i = 1; i < int(tokens.size()) - 1; i += 2)
        {
            const u64 value = std::max<i64>(std::stoll(tokens[i + 1]), 0);

            if ((tokens[i] == "wtime" && mMainSearchThread->mBoard.sideToMove() == Color::WHITE) 
            ||  (tokens[i] == "btime" && mMainSearchThread->mBoard.sideToMove() == Color::BLACK))
                milliseconds = value;

            else if ((tokens[i] == "winc" && mMainSearchThread->mBoard.sideToMove() == Color::WHITE) 
            ||       (tokens[i] == "binc" && mMainSearchThread->mBoard.sideToMove() == Color::BLACK))
                incrementMs = value;

            else if (tokens[i] == "movestogo")
                movesToGo = std::max<u64>(value, 1);
            else if (tokens[i] == "movetime")
            {
                isMoveTime = true;
                milliseconds = value;
            }
            else if (tokens[i] == "depth")
                searchSettings.maxDepth = std::min<u64>(value, MAX_DEPTH);
            else if (tokens[i] == "nodes")
                searchSettings.maxNodes = value;
        }

        // Calculate search time limits

        searchSettings.hardMilliseconds = std::max<i64>(0, (i64)milliseconds - 10);
        searchSettings.softMilliseconds = std::numeric_limits<u64>::max();

        if (!isMoveTime) {
            searchSettings.hardMilliseconds *= hardTimePercentage();
            searchSettings.softMilliseconds = searchSettings.hardMilliseconds * softTimePercentage();
        }

        // Block threads until they're ready
        for (SearchThread searchThread : mSearchThreads)
            searchThread->blockUntilSleep();

        mMainSearchThread->search(searchSettings);

        // Block threads until they're ready
        for (SearchThread searchThread : mSearchThreads)
            searchThread->blockUntilSleep();

        std::cout << "bestmove " << mMainSearchThread->bestMoveRoot().toUci() << std::endl;
    }

}; // class UciEngine
