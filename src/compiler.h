#ifndef ARROW_COMPILER_H
#define ARROW_COMPILER_H

#include "lexer.h"
#include "types.h"
#include "utils.h"

#define MAX_BACKPATCHEES 256

typedef enum {
    OP_NOP,
    OP_PUSH,   OP_ADD,
    OP_SUB,    OP_MUL,
    OP_DIV,    OP_MOD,
    OP_AND,    OP_OR,
    OP_XOR,    OP_SHL,
    OP_SHR,    OP_ROL,
    OP_ROR,    OP_NOT,
    OP_DUP,    OP_OVER,
    OP_DUP2,   OP_DROP,
    OP_SWAP,   OP_OVER2,
    OP_SWAP2,  OP_NEG,
    OP_ABS,    OP_EQ,
    OP_LT,     OP_LTEQ,
    OP_GT,     OP_GTEQ,
    OP_JMPF,   OP_JMP,
    OP_LABEL,  OP_LNOT,
    OP_FUNC,   OP_RET,
    OP_CALL,   OP_STR,
    OP_ROT,    OP_CONVERT,
    OP_CCALL,  OP_ROTN,
    OP_NEQ,    OP_UNKNOWN,
    OP_ACCESS, OP_STORE,
    OP_INIT,   OP_ACCESS_DROP,

    // For the analyser
    OP_START,  OP_END,
    OP_IF,     OP_ELSE,
    OP_ELSEIF,
} Opcode;

typedef enum {
    STYPE_FUNC,  STYPE_MODULE,
    STYPE_TYPE,
} Symbol_Type;

typedef enum {
    UTYPE_OP, UTYPE_TYPE,
} Unresolved_Type;

typedef struct {
    char **input_files;
    char *output_file;
    Cmd link_cmd;
    int input_file_count;
} Compiler_Options;

typedef struct {
    Opcode opcode;
    const char *file_path;
    uint64_t operand;
    Type types[2];
    Loc loc;
} Op;

typedef struct {
    size_t count;
    size_t capacity;
    Op *items;
} Ops;

typedef struct {
    size_t count;
    size_t capacity;
    int *items;
} Dyn_Backpatchees;

typedef struct {
    size_t count;
    int positions[MAX_BACKPATCHEES];
} Backpatchees;

typedef struct {
    String_View name;
    Loc loc;
    Unresolved_Type type;
    union {
        Op *op;
        Type *type;
    } as;
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
    String_View extern_name;
    String_View module_name;
    Types param_types;
    Types return_types;
    int max_allocated;
    uint8_t is_c_func;
} Function;

typedef struct {
    Symbol_Type type;
    union {
        Function func;
        Module module;
        Advanced_Type *type;
    } as;
} Symbol;

typedef struct {
    Arena arena;
    Compiler_Options options;
    Hashmap modules;
    String_Array cleanup;
    uint8_t had_error;
} Compiler;

typedef struct {
    Ops ops;
    Advanced_Types types;
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
char *err_opcode_spelling(Opcode opcode);

#endif // ARROW_COMPILER_H
