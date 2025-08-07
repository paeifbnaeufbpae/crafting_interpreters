#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// forward declare
typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj *obj;
  } as;
} Value;

// these should all be VALUE_IS_FOO instead of IS_FOO

// is a value nil?
#define IS_NIL(value) ((value).type == VAL_NIL)

// is a value a boolean?
#define IS_BOOL(value) ((value).type == VAL_BOOL)

// is a value a number?
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

// is a value an object?
#define IS_OBJ(value) ((value).type == VAL_OBJ)

// assume a value is a boolean
#define AS_BOOL(value) ((value).as.boolean)

// assume a value is a number
#define AS_NUMBER(value) ((value).as.number)

// assume a value is an object
#define AS_OBJ(value) ((value).as.obj)

// lox nil value
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})

// convert a c boolean value to a lox boolean value
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})

// convert a c number value to a lox boolean value
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

// converts an object into a value
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj *)object}})

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif