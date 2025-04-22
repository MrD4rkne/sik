# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

.PHONY: all clean debug

all: peer-time-sync

peer-time-sync: peer-time-sync.o common.o input.o messaging.o packets.o handlers.o domain.o server.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp
common.o: common.cpp common.h
domain.o: domain.cpp domain.h clock.h results.h
handlers.o: handlers.cpp handlers.h domain.h clock.h results.h logging.h \
 messaging.h packets.h
input.o: input.cpp common.h input.h
messaging.o: messaging.cpp messaging.h domain.h clock.h results.h \
 logging.h packets.h
packets.o: packets.cpp packets.h domain.h clock.h results.h logging.h
peer-time-sync.o: peer-time-sync.cpp domain.h clock.h results.h \
 handlers.h logging.h messaging.h packets.h input.h server.h
server.o: server.cpp domain.h clock.h results.h logging.h server.h \
 handlers.h messaging.h packets.h input.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f peer-time-sync *.o