#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

// assume a value is an object and get its type
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// is the value a string object?
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// assume a value is a string object
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))

// assume a value is a string object and then access its char array
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
  OBJ_STRING,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj *next; // next in line in the list of objects that the vm stores
};

// a pointer to a struct is a pointer to the struct's first field, so we can:
// - safely cast an ObjString to an Obj (upcast)
// - safely cast an Obj to and ObjString once we checked its type field (downcast)

struct ObjString {
  Obj obj;
  int length;
  char *chars;

  // cached hash code
  uint32_t hash;
};

ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return (
    IS_OBJ(value) &&
    AS_OBJ(value)->type == type
  );
}

#endif