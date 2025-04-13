# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o err.o common.o input.o messaging.o packets.o handlers.o domain.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp
common.o: common.cpp common.h err.h
domain.o: domain.cpp domain.h results.h clock.h
err.o: err.cpp err.h
handlers.o: handlers.cpp handlers.h domain.h results.h clock.h logging.h \
 messaging.h packets.h
input.o: input.cpp common.h err.h input.h
messaging.o: messaging.cpp messaging.h domain.h results.h clock.h \
 logging.h err.h
packets.o: packets.cpp packets.h domain.h results.h clock.h logging.h
peer-time-sync.o: peer-time-sync.cpp common.h domain.h results.h clock.h \
 err.h handlers.h logging.h messaging.h packets.h input.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o