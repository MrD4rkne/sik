# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Targets and objects
TARGET = peer-time-sync
OBJS = peer-time-sync.o err.o common.o

# Default build target
all: $(TARGET)

# Main executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Object file dependencies
peer-time-sync.o: peer-time-sync.cpp err.h common.h
err.o: err.cpp err.h
common.o: common.cpp common.h

# Pattern rule for compiling .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Debug build with symbols and no optimization
debug: CXXFLAGS += -g -O0
debug: all

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJS)
