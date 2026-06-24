#ifndef ARROW_COMPILER_H
#define ARROW_COMPILER_H

#include "lexer.h"
#include "utils.h"

#define MAX_BACKPATCHEES 256

typedef enum {
    OP_NOP,
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
    OP_CALL,  OP_STR,
    OP_ROT,   OP_CONVERT,
    OP_CCALL, OP_ROTN,
    OP_NEQ,

    // For the analyser
    OP_START, OP_END,
    OP_IF,    OP_ELSE,
} Opcode;

typedef enum {
    STYPE_FUNC, STYPE_MODULE,
} Symbol_Type;

typedef enum {
    TYPE_VOID = 0x0,
    TYPE_I8   = 0x1,   TYPE_CHAR = 0x2,
    TYPE_U8   = 0x4,   TYPE_I16  = 0x8,
    TYPE_U16  = 0x10,  TYPE_I32  = 0x20,
    TYPE_U32  = 0x40,  TYPE_I64  = 0x80,
    TYPE_U64  = 0x100, TYPE_F32  = 0x200,
    TYPE_F64  = 0x400, TYPE_STR  = 0x800,

    TYPE_INTEGER = TYPE_I8  | TYPE_CHAR | TYPE_U8  | TYPE_I16 |
                   TYPE_U16 | TYPE_I32  | TYPE_U32 | TYPE_I64 | TYPE_U64,
    TYPE_REAL    = TYPE_F32 | TYPE_F64,
    TYPE_NUMBER  = TYPE_INTEGER | TYPE_REAL,
} Type;

typedef struct {
    size_t count;
    size_t capacity;
    Type *items;
} Types;

typedef struct {
    char **input_files;
    char *output_file;
    Cmd link_cmd;
} Compiler_Options;

typedef struct {
    Opcode opcode;
    const char *file_path;
    uint64_t operand;
    Type types[2];
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
    String_View module_name;
    Types param_types;
    Types return_types;
    uint8_t is_c_func;
} Function;

typedef struct {
    char *name;
    Type type;
} Field;

typedef struct {
    size_t count;
    size_t capacity;
    Field *items;
} Fields;

typedef struct {
    int size;
    Fields fields;
} Struct;

typedef struct {
    Symbol_Type type;
    union {
        Function func;
        Module module;
        Struct structure;
    } as;
} Symbol;

typedef struct {
    Arena arena;
    Compiler_Options options;
    Hashmap modules;
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

void compile(Compiler_Options options);
char *opcode_spelling(Opcode opcode);

#endif // ARROW_COMPILER_H
