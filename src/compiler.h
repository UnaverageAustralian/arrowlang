#ifndef ARROW_COMPILER_H
#define ARROW_COMPILER_H

#include "lexer.h"
#include "utils.h"

#define MAX_BACKPATCHEES 256

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
    OP_LABEL, OP_LNOT,
    OP_FUNC,  OP_RET,
    OP_CALL,
} Opcode;

typedef enum {
    STYPE_FUNC, STYPE_MODULE,
} Symbol_Type;

typedef struct {
    char *file_path;
    char *output_file;
    String_Array linker_files;
} Compiler_Options;

typedef struct {
    Opcode opcode;
    const char *file_path;
    uint64_t operand;
    int pos, line;
} Op;

typedef struct {
    size_t count;
    size_t capacity;
    Op *items;
} Ops;

typedef struct {
    size_t count;
    int positions[MAX_BACKPATCHEES];
} Backpatchees;

typedef struct {
    int arity, ret_arity;
} Function;

typedef struct {
    String_View name;
    int pos;
} Unresolved_Symbol;

typedef struct {
    size_t count;
    size_t capacity;
    Unresolved_Symbol *items;
} Unresolved_Symbols;

typedef struct {
    String_View path;
    String_View name;
    Hashmap symbols;
    uint8_t has_ext_funcs;
} Module;

typedef struct {
    Symbol_Type type;
    union {
        Function func;
        Module module;
    } as;
} Symbol;

typedef struct {
    Arena arena;
    Compiler_Options options;
    uint8_t had_error;
} Compiler;

typedef struct {
    Ops ops;
    Hashmap symbols;
    Unresolved_Symbols unresolved;
    Module module;
    Compiler *global;
    Lexer *lexer;
    Backpatchees brks;
    Backpatchees conts;
    Backpatchees rets;
    int label_count;
    uint8_t is_in_loop;
} Compilation_Unit;

void compile(const char *src, Compiler_Options options);
void print_op(Op *op);

#endif // ARROW_COMPILER_H
