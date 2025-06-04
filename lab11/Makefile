CC      = gcc
CFLAGS  = -z execstack -Wall -Wextra -O2
LDFLAGS =

TARGETS = echo-server-nonblocking

.PHONY: all clean

all: $(TARGETS)

echo-server-nonblocking: echo-server-nonblocking.o err.o common.o

# To są zależności wygenerowane automatycznie za pomocą polecenia `gcc -MM *.c`.

common.o: common.c err.h common.h
echo-server-nonblocking.o: echo-server-nonblocking.c err.h common.h
err.o: err.c err.h

clean:
	rm -f $(TARGETS) *.o
