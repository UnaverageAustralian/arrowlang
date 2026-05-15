#ifndef ARROW_COMPILER_H
#define ARROW_COMPILER_H

#include "lexer.h"

#define MAX_BACKPATCHEES 512

typedef enum {
    OP_PUSH,  OP_ADD,
    OP_SUB,   OP_MUL,
    OP_DIV,   OP_MOD,
    OP_AND,   OP_OR,
    OP_XOR,   OP_SHL,
    OP_SHR,   OP_ROL,
    OP_ROR,   OP_NOT,
    OP_DUP,   OP_OVER,
    OP_DUP2,  OP_DROP,
    OP_SWAP,  OP_OVER2,
    OP_SWAP2, OP_NEG,
    OP_ABS,   OP_EQ,
    OP_LT,    OP_LTEQ,
    OP_GT,    OP_GTEQ,
    OP_JMPF,  OP_JMP,
    OP_LABEL, OP_LNOT
} Opcode;

typedef enum {
    BTYPE_BRK, BTYPE_CONT,
} Backpatchee_Type;

typedef struct {
    Opcode opcode;
    const char *file_path;
    int64_t operand;
    int pos, line;
} Op;

typedef struct {
    size_t count;
    size_t capacity;
    Op *items;
} Ops;

typedef struct {
    int pos;
    Backpatchee_Type type;
} Backpatchee;

typedef struct {
    Ops ops;
    Lexer *lexer;
    Backpatchee backpatchees[MAX_BACKPATCHEES];
    int backpatchees_count;
    int label_count;
    uint8_t had_error;
    uint8_t is_in_loop;
} Compiler;

void compile(const char *src, const char *file_path);
void print_op(Op *op);

#endif // ARROW_COMPILER_H
