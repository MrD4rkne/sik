CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17
LFLAGS =

.PHONY: all clean

TARGET1 = echo-client
TARGET2 = echo-server

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(TARGET1).o err.o
$(TARGET2): $(TARGET2).o err.o

# To są zależności wygenerowane automatycznie za pomocą polecenia `gcc -MM *.c`.
echo-client.o: echo-client.c err.h
echo-server.o: echo-server.c err.h
err.o: err.c err.h

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
