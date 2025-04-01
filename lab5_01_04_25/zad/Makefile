CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17 -I../przyklady
LDFLAGS =

vpath %.c ../przyklady
vpath %.h ../przyklady

.PHONY: all clean

TARGET1 = file-client-tcp
TARGET2 = file-server-tcp

all: $(TARGET1) $(TARGET2) $(TARGET3)

$(TARGET1): $(TARGET1).o err.o common.c

$(TARGET2): $(TARGET2).o err.o common.c
	$(CC) $^ -o $@ -lpthread

common.o: common.c err.h common.h
err.o: err.c err.h
file-client-tcp.o: file-client-tcp.c err.h common.h protocol.h
file-server-tcp.o: file-server-tcp.c err.h common.h protocol.h

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
