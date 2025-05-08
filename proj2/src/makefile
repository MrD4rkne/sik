# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

TARGETS = approx-server approx-client

.PHONY: all clean debug

all: $(TARGETS)

approx-server: approx-server.o input.o network.o
	$(CXX) $(CXXFLAGS) -o $@ $^

approx-client: approx-client.o input.o network.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp

approx-client.o: approx-client.cpp input.h network.h
approx-server.o: approx-server.cpp input.h network.h
input.o: input.cpp input.h
network.o: network.cpp

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o