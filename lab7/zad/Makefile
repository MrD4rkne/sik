C      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu17
LDFLAGS = -Wall

TARGETS = rozglaszacz

all: $(TARGETS)

rozglaszacz: rozglaszacz.o cb.o err.o

cb.o: cb.c cb.h err.h
err.o: err.c err.h
rozglaszacz.o: rozglaszacz.c cb.h err.h

.PHONY: clean

clean:
	rm -f $(TARGETS) *.o *~
