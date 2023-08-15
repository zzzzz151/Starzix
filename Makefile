CC := g++
CFLAGS := -O3 -g -std=c++17

# Directories
SRC_DIR := src

# Source files and object files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, %.o, $(SRCS))

# Executables
EXECUTABLES := z5 test

# Default target
all: $(EXECUTABLES)

z5: $(SRC_DIR)/main.cpp
	$(CC) $(CFLAGS) $< -o $@

test: $(SRC_DIR)/test.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLES)

.PHONY: all clean