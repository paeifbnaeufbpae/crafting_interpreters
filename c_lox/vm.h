#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;

  // instruction pointer
  // points to the *next* instruction, not the current one
  uint8_t *ip;

  Value stack[STACK_MAX];
  Value *stackTop; // exclusive (one after the last element)

  // interned strings (hash set)
  Table strings;

  // linked list of objects
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);
Value pop();

#endif