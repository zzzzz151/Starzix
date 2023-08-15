CC := g++
CFLAGS := -O3 -g -std=c++17

# Source files and object files
SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, %.o, $(SRCS))

# Default target
all: z5

z5: src/main.cpp
	$(CC) $(CFLAGS) $< -o $@

# Source files and object files
SRCS := $(wildcard src-test/*.cpp)
OBJS := $(patsubst src-test/%.cpp, %.o, $(SRCS))

test: src-test/main.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f z5 test

.PHONY: all clean