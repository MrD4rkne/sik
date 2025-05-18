# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

TARGETS = approx-server approx-client

.PHONY: all clean debug

all: $(TARGETS)

approx-server: approx-server.o input.o network.o ip.o fd.o concaters.o
	$(CXX) $(CXXFLAGS) -o $@ $^

approx-client: approx-client.o input.o network.o ip.o client.o fd.o concaters.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Generated with
# g++ -MM *.cpp

approx-client.o: approx-client.cpp client.h handlers.h ip.h logging.h \
 network.h fd.h messages.h concaters.h results.h input.h
approx-server.o: approx-server.cpp input.h ip.h logging.h network.h fd.h \
 messages.h concaters.h server.h file.h
client.o: client.cpp client.h handlers.h ip.h logging.h network.h fd.h \
 messages.h concaters.h results.h
concaters.o: concaters.cpp concaters.h
fd.o: fd.cpp fd.h
input.o: input.cpp input.h
ip.o: ip.cpp ip.h
network.o: network.cpp network.h fd.h ip.h logging.h messages.h \
 concaters.h

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o