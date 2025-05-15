# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

TARGETS = approx-server approx-client

.PHONY: all clean debug

all: $(TARGETS)

approx-server: approx-server.o input.o network.o ip.o
	$(CXX) $(CXXFLAGS) -o $@ $^

approx-client: approx-client.o input.o network.o ip.o client.o fd.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp

approx-client.o: approx-client.cpp client.h handlers.h ip.h logging.h \
 network.h messages.h fd.h results.h input.h
approx-server.o: approx-server.cpp input.h ip.h network.h logging.h \
 messages.h fd.h
client.o: client.cpp client.h handlers.h ip.h logging.h network.h \
 messages.h fd.h results.h
fd.o: fd.cpp fd.h
input.o: input.cpp input.h
ip.o: ip.cpp ip.h
network.o: network.cpp network.h ip.h logging.h messages.h fd.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o