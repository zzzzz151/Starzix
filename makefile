CXX := clang++
WARNINGS := -Wall -Wextra -Werror -Wunused -Wconversion -Wsign-conversion -Wshadow -Wpedantic -Wold-style-cast
CXXFLAGS := -std=c++23 -O3 -flto -fuse-ld=lld -funroll-loops -fconstexpr-steps=100000000 $(WARNINGS)
SUFFIX :=

ifeq ($(OS), Windows_NT)
	EXE ?= Starzix

	ENDS_WITH := exe
	ifneq ($(patsubst %$(ENDS_WITH),,$(lastword $(EXE))),)
		SUFFIX := .exe
	endif
else
	EXE ?= starzix
	CXXFLAGS += -lstdc++ -lm
endif

all:
	$(CXX) $(CXXFLAGS) -march=native -DNDEBUG src/*.cpp -o $(EXE)$(SUFFIX)
debug:
	$(CXX) $(CXXFLAGS) -march=native src/*.cpp -o $(EXE)$(SUFFIX)
testPosition:
	$(CXX) $(CXXFLAGS) -march=native tests/test_position.cpp -o test-position$(SUFFIX)
	./test-position$(SUFFIX)
testMoveGen:
	$(CXX) $(CXXFLAGS) -march=native tests/test_move_gen.cpp -o test-move-gen$(SUFFIX)
	./test-move-gen$(SUFFIX)
testGameState:
	$(CXX) $(CXXFLAGS) -march=native tests/test_game_state.cpp -o test-game-state$(SUFFIX)
	./test-game-state$(SUFFIX)
testSEE:
	$(CXX) $(CXXFLAGS) -march=native -DTUNE tests/test_SEE.cpp -o test-SEE$(SUFFIX)
	./test-SEE$(SUFFIX)
testNNUE:
	$(CXX) $(CXXFLAGS) -march=native tests/test_NNUE.cpp -o test-NNUE$(SUFFIX)
	./test-NNUE$(SUFFIX)
tune:
	$(CXX) $(CXXFLAGS) -march=native -DNDEBUG -DTUNE src/*.cpp -o $(EXE)$(SUFFIX)
release:
	$(CXX) $(CXXFLAGS) -march=x86-64-v3 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx2$(SUFFIX)
	$(CXX) $(CXXFLAGS) -march=x86-64-v4 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx512$(SUFFIX)
