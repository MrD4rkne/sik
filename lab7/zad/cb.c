#include "cb.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"

static const size_t CB_INITIAL_SIZE = 16384;

void cbInit(CircularBuffer *b)
{
  b->buf = malloc(CB_INITIAL_SIZE);
  if (!b->buf)
    fatal("Out of memory.");
  b->pos = 0;
  b->capacity = CB_INITIAL_SIZE;
  b->size = 0;
}

void cbDestroy(CircularBuffer *b)
{
  if (b->buf) {
    free(b->buf);
    b->buf = NULL;
  }
  b->size = 0;
  b->capacity = 0;
  b->pos = 0;
}

void cbPushBack(CircularBuffer *b, char const *data, size_t n)
{
  if (n + b->size > b->capacity) {
    size_t oldCapacity = b->capacity;
    size_t cm = 2;
    while (n + b->size > cm * b->capacity)
      cm *= 2;
    void *newBuf = reallocarray(b->buf, cm, b->capacity);
    if (!newBuf)
      fatal("Out of memory.");
    b->buf = newBuf;
    b->capacity = cm * oldCapacity;

    if (b->pos + b->size > oldCapacity)
      memmove(b->buf + oldCapacity, b->buf, b->pos + b->size - oldCapacity);
    if (b->pos) {
      memmove(b->buf, b->buf + b->pos, b->size);
      b->pos = 0;
    }
  }

  char *dataEnd = b->buf + ((b->pos + b->size) % b->capacity);
  size_t spaceAtEnd = b->capacity - (dataEnd - b->buf);
  memcpy(dataEnd, data, n > spaceAtEnd ? spaceAtEnd : n);
  if (n > spaceAtEnd)
    memcpy(b->buf, data + spaceAtEnd, n - spaceAtEnd);
  b->size += n;
}

void cbDropFront(CircularBuffer *b, size_t n)
{
  assert(n <= b->size);

  b->pos = (b->pos + n) % b->capacity;
  b->size -= n;
}

size_t cbGetContinuousCount(CircularBuffer const *b)
{
  size_t possible = b->capacity - b->pos;
  return possible > b->size ? b->size : possible;
}

char *cbGetData(CircularBuffer const *b)
{
  return b->buf + b->pos;
}
