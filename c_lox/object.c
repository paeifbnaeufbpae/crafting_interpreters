#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(
  size_t size,
  ObjType type
) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);

  object->type = type;

  object->next = vm.objects;
  vm.objects = object;

  return object;
}

static ObjString *allocateString(
  char *chars,
  int length,
  uint32_t hash
) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);

  string->length = length;
  string->chars = chars;
  string->hash = hash;

  tableSet(
    &vm.strings,
    string,
    NIL_VAL // we don't need a value, we just care about the key (it's a set)
  );

  return string;
}

// fnv-1a
static uint32_t hashString(
  const char *key,
  int length
) {
  uint32_t hash = 2166136261u;

  for (
    int i = 0;
    i < length;
    i ++
  ) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

// takes ownership of the string
ObjString *takeString(
  char *chars,
  int length
) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(
    &vm.strings,
    chars,
    length,
    hash
  );

  if (interned != NULL) {
    // ownership of chars is being passed to this function and we no longer need the duplicate string, so it's up to us to free it
    // we have a duplicate string e.g. when we concatenate two strings and receive a third one that already exists, so we free that one here
    FREE_ARRAY(
      char,
      chars,
      length + 1
    );

    return interned;
  }

  return allocateString(chars, length, hash);
}

// take a slice of a string and return the (possibly new) interned string object for it
ObjString *copyString(
  const char *chars,
  int length
) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(
    &vm.strings,
    chars,
    length,
    hash
  );

  if (interned != NULL) return interned; // that string already exists

  char *heapChars = ALLOCATE(char, length + 1);
  
  memcpy(
    heapChars,
    chars,
    length
  );

  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
  }
}