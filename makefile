CXX := clang++
WARNINGS := -Wall -Wextra -Werror -Wunused -Wconversion -Wsign-conversion -Wshadow -Wpedantic -Wold-style-cast
CXXFLAGS := -std=c++23 -O3 -funroll-loops -flto -fuse-ld=lld -fconstexpr-steps=100000000 $(WARNINGS)
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
	$(CXX) $(CXXFLAGS) -march=native src/*.cpp -o $(EXE)$(SUFFIX)
test:
	$(CXX) $(CXXFLAGS) -march=native tests/testPosition.cpp -o testPosition$(SUFFIX)
	./testPosition$(SUFFIX)

	$(CXX) $(CXXFLAGS) -march=native tests/testMoveGen.cpp -o testMoveGen$(SUFFIX)
	./testMoveGen$(SUFFIX)

	$(CXX) $(CXXFLAGS) -march=native tests/testGameState.cpp -o testGameState$(SUFFIX)
	./testGameState$(SUFFIX)

	$(CXX) $(CXXFLAGS) -march=native tests/testSEE.cpp -o testSEE$(SUFFIX)
	./testSEE$(SUFFIX)

	$(CXX) $(CXXFLAGS) -march=native tests/testNNUE.cpp -o testNNUE$(SUFFIX)
	./testNNUE$(SUFFIX)
testMovegen:
	$(CXX) $(CXXFLAGS) -march=native tests/testMoveGen.cpp -o testMoveGen$(SUFFIX)
	./testMoveGen$(SUFFIX)
tune:
	$(CXX) $(CXXFLAGS) -march=native -DNDEBUG -DTUNE src/*.cpp -o $(EXE)$(SUFFIX)
release:
	$(CXX) $(CXXFLAGS) -march=x86-64-v3 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx2$(SUFFIX)
	$(CXX) $(CXXFLAGS) -march=x86-64-v4 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx512$(SUFFIX)
