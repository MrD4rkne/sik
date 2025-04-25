CC = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic
TARGET = hello

all: $(TARGET)

$(TARGET): $(TARGET).cpp ../common.h
	$(CC) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean