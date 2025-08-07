#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;

  initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {

  FREE_ARRAY(
    uint8_t,
    chunk->code,
    chunk->capacity
  );

  FREE_ARRAY(
    int,
    chunk->lines,
    chunk->capacity
  );

  freeValueArray(&chunk->constants);

  // zero out the fields and leave the chunk in a well-defined "empty" state
  initChunk(chunk);
}

void writeChunk(
  Chunk *chunk,
  uint8_t byte,
  int line
) {
  // enough capacity to write one more byte?
  if (chunk->capacity < chunk->count + 1) {

    int oldCapacity = chunk->capacity;

    chunk->capacity = GROW_CAPACITY(oldCapacity);

    chunk->code = GROW_ARRAY(
      uint8_t,
      chunk->code,
      oldCapacity,
      chunk->capacity
    );

    chunk->lines = GROW_ARRAY(
      int,
      chunk->lines,
      oldCapacity,
      chunk->capacity
    );
  }

  // last element is at chunk->count - 1
  chunk->code[chunk->count] = byte;

  chunk->lines[chunk->count] = line;

  chunk->count++;
}

// returns the index in the constants table of the appended constant
int addConstant(
  Chunk *chunk,
  Value value
) {
  writeValueArray(&chunk->constants, value);

  return chunk->constants.count - 1;
}