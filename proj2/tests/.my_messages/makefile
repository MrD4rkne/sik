# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

TARGETS = test

.PHONY: all clean debug

all: $(TARGETS)

test: message_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp

message_test.o: message_test.cpp messages.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o