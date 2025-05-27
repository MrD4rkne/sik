# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -MMD

BINS = approx-server approx-client

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)
DEPS = $(OBJECTS:.o=.d)

all: $(BINS)

-include $(DEPS)

$(BINS): %: %.o $(filter-out $(BINS:=.o), $(OBJECTS))
	$(CXX) -o $@ $^

clean:
	rm -f $(OBJECTS) $(DEPS) $(BINS)

.PHONY: all clean

DEBUG_FLAGS = -DDEBUG -g -O0
debug: CXXFLAGS := $(filter-out -O2, $(CXXFLAGS)) $(DEBUG_FLAGS)
debug: all