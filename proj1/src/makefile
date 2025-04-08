# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o err.o common.o input.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# $ g++ -MM *.cpp
common.o: common.cpp err.h common.h
err.o: err.cpp err.h
input.o: input.cpp input.h common.h err.h
peer-time-sync.o: peer-time-sync.cpp err.h common.h input.h logging.h \
 domain.h messaging.h packets.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o