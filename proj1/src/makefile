# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o err.o common.o input.o messaging.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# $ g++ -MM *.cpp
common.o: common.cpp common.h err.h
err.o: err.cpp err.h
input.o: input.cpp common.h err.h input.h
messaging.o: messaging.cpp messaging.h domain.h packets.h logging.h err.h
peer-time-sync.o: peer-time-sync.cpp common.h domain.h err.h input.h \
 logging.h messaging.h packets.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o