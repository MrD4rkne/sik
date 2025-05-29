# Great thanks to M. Birecki for this awesome makefile.

# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -MMD

BINS = test_force_close

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