#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

// low to high
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR, // or
  PREC_AND, // and
  PREC_EQUALITY, // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM, // + -
  PREC_FACTOR, // * /
  PREC_UNARY, // ! -
  PREC_CALL, // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  // all locals that are in scope (source order)
  Local locals[UINT8_COUNT];

  // how many slots in the above array are in use
  int localCount;

  // number of blocks we are currently surrounded by
  int scopeDepth;

  // number variables with the level of nesting where they appear => track which block each local belongs to so that we know WHICH LOCALS TO DISCARD WHEN A BLOCK ENDS (exactly my problem)
} Compiler;

Parser parser;
Compiler *current = NULL;
Chunk *compilingChunk;

static Chunk *currentChunk() {
  return compilingChunk;
}

static void errorAt(
  Token *token,
  const char *message
) {
  if (parser.panicMode) return;

  parser.panicMode = true;

  fprintf(stderr, "[line %d] error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // nothing
  } else {
    fprintf(stderr, "at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);

  parser.hadError = true;
}

static void error(const char *message) {
  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(
  TokenType type,
  const char *message
) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;

  advance();

  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(
    currentChunk(),
    byte,
    parser.previous.line
  );
}

static void emitBytes(
  uint8_t byte1,
  uint8_t byte2
) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitReturn() {
  emitByte(OP_RETURN);
}

// adds the value as a constant to the current chunk and returns the index of that new constant in the chunk's constants table
static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);

  if (constant > UINT8_MAX) {
    error("too many constants in one chunk");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  current = compiler;
}

static void endCompiler() {
  emitReturn();

#ifdef DEBUG_PRINT_CODE

  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }

#endif

}

// forward declarations
static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

// don't know why this is not in the book, maybe i made some mistake... but it's necessary
static uint8_t identifierConstant(Token *name);

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  ParseRule *rule = getRule(operatorType);

  // +1 for left-associative, +0 for right-associative
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
    case TOKEN_GREATER: emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS: emitByte(OP_ADD); break;
    case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
    default: return; // unreachable
  }
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    default: return; // unreachable
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "expect ')' after expression");
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(copyString(
    parser.previous.start + 1,
    parser.previous.length - 2
  )));
}

static void namedVariable(
  Token name,
  bool canAssign
) {
  uint8_t arg = identifierConstant(&name);

  if (
    canAssign &&
    match(TOKEN_EQUAL)
  ) {
    expression();
    emitBytes(OP_SET_GLOBAL, arg);
  } else {
    emitBytes(OP_GET_GLOBAL, arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  // compile the operand
  parsePrecedence(PREC_UNARY);

  // emit the operator instruction
  switch (operatorType) {
    case TOKEN_BANG: emitByte(OP_NOT); break;
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    default: return; // unreachable
  }
}

// makes it very easy to see which tokens are in use by the grammar and which are available
ParseRule rules[] = {
  [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
  [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
  [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
  [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
  [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
  [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
  [TOKEN_MINUS] = {unary, binary, PREC_TERM},
  [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
  [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
  [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
  [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
  [TOKEN_BANG] = {unary, NULL, PREC_NONE},
  [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
  [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
  [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
  [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
  [TOKEN_STRING] = {string, NULL, PREC_NONE},
  [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
  [TOKEN_AND] = {NULL, NULL, PREC_NONE},
  [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
  [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
  [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
  [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
  [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
  [TOKEN_IF] = {NULL, NULL, PREC_NONE},
  [TOKEN_NIL] = {literal, NULL, PREC_NONE},
  [TOKEN_OR] = {NULL, NULL, PREC_NONE},
  [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
  [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
  [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
  [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
  [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
  [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
  [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
  [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
  [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    error("expect expression");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;

  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();

    ParseFn infixRule = getRule(parser.previous.type)->infix;

    infixRule(canAssign);
  }

  if (
    canAssign &&
    match(TOKEN_EQUAL)
  ) {
    error("invalid assignment target");
  }
}

// turn an identifier into a constant and add it to the constants table, returning the index
static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(
    name->start,
    name->length
  )));
}

static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
  emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule *getRule(TokenType type) {
  return &rules[type];
}

static void expression() {
  // parse lowest precedence level
  parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
  uint8_t global = parseVariable("expect variable name");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "expect ';' after variable declaration");

  defineVariable(global);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "expect ';' after expession");
  emitByte(OP_POP);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "expect ';' after value");
  emitByte(OP_PRINT);
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {

    // i guess this is for something like, say, `var;`, where more was expected, but we still assume the semicolon is meaningful and is intended to end the statement
    if (parser.previous.type == TOKEN_SEMICOLON) return;

    switch (parser.current.type) {
      // these are all valid starting points for a new declarations or statements
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // do nothing
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode) synchronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else {
    expressionStatement();
  }
}

// returns whether or not compilation succeeded
bool compile(
  const char *source,
  Chunk *chunk
) {
  initScanner(source);

  Compiler compiler;
  initCompiler(&compiler);

  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance();
  
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  endCompiler();

  return !parser.hadError;
}