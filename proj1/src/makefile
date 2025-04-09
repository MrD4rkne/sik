# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o err.o common.o input.o messaging.o packets.o handlers.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp
common.o: common.cpp common.h err.h
err.o: err.cpp err.h
handlers.o: handlers.cpp handlers.h domain.h logging.h packets.h \
 messaging.h
input.o: input.cpp common.h err.h input.h
messaging.o: messaging.cpp messaging.h domain.h packets.h logging.h err.h
packets.o: packets.cpp packets.h domain.h logging.h
peer-time-sync.o: peer-time-sync.cpp common.h domain.h err.h input.h \
 logging.h messaging.h packets.h handlers.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o