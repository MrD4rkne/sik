# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o err.o common.o input.o
	$(CXX) $(CXXFLAGS) -o $@ $^

peer-time-sync.o: peer-time-sync.cpp err.h common.h
err.o: err.cpp err.h
common.o: common.cpp common.h
input.o: input.cpp input.h

debug: CXXFLAGS += -g -O0
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o