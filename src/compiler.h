#ifndef ARROW_COMPILER_H
#define ARROW_COMPILER_H

#include "lexer.h"
#include "types.h"
#include "utils.h"
#include <stdint.h>

#define MAX_BACKPATCHEES 256

typedef enum {
    OP_NOP,
    OP_PUSH,        OP_ADD,
    OP_SUB,         OP_MUL,
    OP_DIV,         OP_MOD,
    OP_AND,         OP_OR,
    OP_XOR,         OP_SHL,
    OP_SHR,         OP_ROL,
    OP_ROR,         OP_NOT,
    OP_DUP,         OP_OVER,
    OP_DUP2,        OP_DROP,
    OP_SWAP,        OP_OVER2,
    OP_SWAP2,       OP_NEG,
    OP_ABS,         OP_EQ,
    OP_LT,          OP_LTEQ,
    OP_GT,          OP_GTEQ,
    OP_JMPF,        OP_JMP,
    OP_LABEL,       OP_LNOT,
    OP_FUNC,        OP_RET,
    OP_CALL,        OP_STR,
    OP_ROT,         OP_CONVERT,
    OP_CCALL,       OP_ROTN,
    OP_NEQ,         OP_UNKNOWN,
    OP_ACCESS,      OP_STORE,
    OP_INIT,        OP_ACCESS_DROP,
    OP_PTR_STORE,   OP_PTR_ACCESS,
    OP_INDEX,       OP_INDEX_STORE,
    OP_ALLOC,       OP_PTR_ACCESS_DROP,
    OP_LDROP,       OP_PUSH_GLOBAL,
    OP_GLOBAL,      OP_CALL_MACRO,
    OP_ALLOC_STORE,

    // For the analyser
    OP_START,  OP_END,
    OP_IF,     OP_ELSE,
    OP_ELSEIF, OP_SIZEOF,
    OP_RETURN, OP_MACRO,

    OP_LAST = OP_MACRO
} Opcode;

typedef enum {
    STYPE_FUNC,   STYPE_MODULE,
    STYPE_TYPE,   STYPE_CONST,
    STYPE_GLOBAL, STYPE_MACRO,
} Symbol_Type;

typedef enum {
    UTYPE_OP, UTYPE_TYPE,
} Unresolved_Type;

typedef enum {
    DIR_START, DIR_END,
    DIR_LINK,

    DIR_LAST = DIR_LINK,
} Directive_Type;

typedef enum {
    ATTR_PRIVATE,
    ATTR_INIT, ATTR_FINI,

    ATTR_LAST = ATTR_FINI,
} Attribute_Type;

typedef struct {
    String_View compiler_dir;
    char **input_files;
    char *output_file;
    Cmd link_cmd;
    int input_file_count;
    uint8_t verbose  : 1;
    uint8_t emit_asm : 1;
    uint8_t emit_obj : 1;
    uint8_t debug    : 1;
    uint8_t print_ir : 1;
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
        size_t op;
        Type *type;
    } as;
} Unresolved_Symbol;

typedef struct {
    size_t count;
    size_t capacity;
    Unresolved_Symbol *items;
} Unresolved_Symbols;

typedef struct Module Module;
struct Module {
    Module *parent;
    String_View path;
    String_View name;
    String_View full_name;
    Hashmap symbols;
    Resolve_Status status;
};

typedef struct {
    Types param_types;
    Types return_types;
    union {
        struct {
            String_View extern_name;
            String_View module_name;
        };
        Ops ops;
    };
    int max_allocated;
    uint8_t is_c_func;
    uint8_t is_macro;
} Function;

typedef struct {
    uint64_t val;
    Type type;
} Constant;

typedef struct {
    String_View name;
    String_View module_name;
    Type type;
} Global;

typedef struct {
    size_t count;
    size_t capacity;
    Global *items;
} Globals;

typedef union {
    struct {
        uint8_t private : 1;
        uint8_t init : 1;
        uint8_t fini : 1;
    };
    uint64_t value;
} Attributes;

typedef struct {
    Symbol_Type type;
    Attributes attributes;
    union {
        Function func;
        Module module;
        Advanced_Type *type;
        Constant constant;
        Global global;
    } as;
} Symbol;

typedef struct {
    Arena arena;
    Compiler_Options options;
    Hashmap modules;
    String_Array cleanup;
    int file;
    uint8_t had_error;
} Compiler;

typedef struct {
    Ops ops;
    Advanced_Types types;
    Unresolved_Symbols unresolved;
    Module *module;
    Compiler *global;
    Lexer *lexer;
    Backpatchees brks;
    Backpatchees conts;
    Backpatchees rets;
    int label_count;
    Attributes global_attrs;
    Attributes sym_attrs;
    uint8_t is_in_loop;
} Compilation_Unit;

void compile(Compiler_Options options);
const char *opcode_spelling(Opcode opcode);

#endif // ARROW_COMPILER_H
