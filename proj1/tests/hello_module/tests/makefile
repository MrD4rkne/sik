CC = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic
TARGET = empty_hello_response

all: $(TARGET)

$(TARGET): $(TARGET).cpp ../../common.h
	$(CC) $(CXXFLAGS) -o $@ $<

log: $(TARGET).cpp ../../common.h
	$(CC) $(CXXFLAGS) -DLOG -o $(TARGET) $<

clean:
	rm -f $(TARGET)

.PHONY: all clean