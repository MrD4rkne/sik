# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all:

# peer-time-sync: *.o
# 	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f approx-server approx-client *.o