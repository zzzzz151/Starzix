CXXFLAGS = -std=c++23 -O3 -funroll-loops -flto -fuse-ld=lld -fno-exceptions -Wunused -Wall -Wextra -fconstexpr-steps=100000000
SUFFIX =

ifeq ($(OS), Windows_NT)
	EXE ?= Starzix
	CLANG_PLUS_PLUS_18 = $(shell where clang++-18 > NUL 2>&1)

	ENDS_WITH := exe
	ifneq ($(patsubst %$(ENDS_WITH),,$(lastword $(EXE))),)
		SUFFIX = .exe
	endif
else
	EXE ?= starzix
	CLANG_PLUS_PLUS_18 = $(shell command -v clang++-18 2>/dev/null)
	CXXFLAGS += -lstdc++ -lm
endif

ifeq ($(strip $(CLANG_PLUS_PLUS_18)),)
    COMPILER = clang++
else
    COMPILER = clang++-18
endif

all:
	$(COMPILER) $(CXXFLAGS) -march=native -DNDEBUG src/*.cpp -o $(EXE)$(SUFFIX)
test:
	$(COMPILER) $(CXXFLAGS) -march=native tests/tests.cpp -o testsCore$(SUFFIX)
	$(COMPILER) $(CXXFLAGS) -march=native -DTUNE tests/testsSEE.cpp -o testsSEE$(SUFFIX)
	$(COMPILER) $(CXXFLAGS) -march=native tests/testsNNUE.cpp -o testsNNUE$(SUFFIX)
tune:
	$(COMPILER) $(CXXFLAGS) -march=native -DNDEBUG -DTUNE src/*.cpp -o $(EXE)$(SUFFIX)
release:
	$(COMPILER) $(CXXFLAGS) -march=x86-64-v3 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx2$(SUFFIX)
	$(COMPILER) $(CXXFLAGS) -march=x86-64-v4 -DNDEBUG -pthread -static -Wl,--no-as-needed src/*.cpp -o $(EXE)-avx512$(SUFFIX)
	