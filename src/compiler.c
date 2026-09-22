#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "analyser.h"
#include "compiler.h"
#include "gen.h"
#include "lexer.h"
#include "types.h"
#include "utils.h"

#define COMPILER_EPRINTF(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->prev.loc, level, __VA_ARGS__); 
#define COMPILER_EPRINTF_AT_CUR(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->cur.loc, level, __VA_ARGS__);

#define START_DECL(sym_type)                                                                                      \
    do {                                                                                                          \
        Attributes sym_attrs = (Attributes){ .value = compiler->global_attrs.value | compiler->sym_attrs.value }; \
        if (sym_attrs.value & ~valid.value) {                                                                     \
            compiler->global->had_error = 1;                                                                      \
            COMPILER_EPRINTF(LEVEL_ERR, "Symbol has invalid attributes\n");                                       \
        }                                                                                                         \
        lexer_next(compiler->lexer);                                                                              \
        expect(compiler, TOK_WORD);                                                                               \
        if (compiler->lexer->prev.type == TOK_EOF) return;                                                        \
        sym = arena_calloc(&compiler->global->arena, sizeof(Symbol));                                             \
        sym->type = sym_type;                                                                                     \
        sym->attributes = sym_attrs;                                                                              \
        add_symbol(compiler, sym);                                                                                \
    } while (0)

static const char *opcodes[] = {
    "NOP",
    "PUSH",        "ADD",
    "SUB",         "MUL",
    "DIV",         "MOD",
    "AND",         "OR",
    "XOR",         "SHL",
    "SHR",         "ROL",
    "ROR",         "NOT",
    "DUP",         "OVER",
    "DUP2",        "DROP",
    "SWAP",        "OVER2",
    "SWAP2",       "NEG",
    "ABS",         "EQ",
    "LT",          "LTEQ",
    "GT",          "GTEQ",
    "JMPF",        "JMP",
    "LABEL",       "LNOT",
    "FUNC",        "RET",
    "CALL",        "STR",
    "ROT",         "CONVERT",
    "CCALL",       "ROTN",
    "NEQ",         "UNKNOWN",
    "ACCESS",      "STORE",
    "INIT",        "ACCESS_DROP",
    "PTR_STORE",   "PTR_ACCESS",
    "INDEX",       "INDEX_STORE",
    "ALLOC",       "PTR_ACCESS_DROP",
    "LDROP",       "PUSH_GLOBAL",
    "GLOBAL",      "CALL_MACRO",
    "ALLOC_STORE",

    "START",  "END",
    "IF",     "ELSE",
    "ELSEIF", "SIZEOF",
    "RETURN", "MACRO",
};
static_assert(sizeof(opcodes)/sizeof(const char *)-1 == OP_LAST, "Update opcodes table in compiler");

static Opcode tok_to_opcode[] = {
    OP_NOP,
    OP_PUSH,        OP_PUSH,
    OP_PUSH,        OP_STR,
    OP_NOP,

    OP_ADD,         OP_SUB,
    OP_MUL,         OP_DIV,
    OP_NOP,         OP_LNOT,
    OP_NOP,         OP_NOP,
    OP_PTR_ACCESS,  OP_NOP,
    OP_NOP,         OP_NOP,
    OP_NOP,         OP_NOP,
    OP_NOP,         OP_NOP,
    OP_ACCESS,      OP_STORE,
    OP_ACCESS_DROP, OP_NOP,
    OP_NOP,         OP_NOP,
    OP_PTR_STORE,   OP_INDEX,
    OP_INDEX_STORE, OP_PTR_ACCESS_DROP,
    OP_EQ,

    OP_MOD,         OP_AND,
    OP_OR,          OP_XOR,
    OP_SHL,         OP_SHR,
    OP_ROL,         OP_ROR,
    OP_NOT,         OP_SWAP,
    OP_LT,          OP_LTEQ,
    OP_GT,          OP_GTEQ,
    OP_NEQ,

    OP_DUP,         OP_OVER,
    OP_DUP2,        OP_OVER2,
    OP_SWAP2,       OP_NOP,
    OP_NOP,         OP_NOP,
    OP_JMP,         OP_JMP,
    OP_NOP,         OP_INIT,
    OP_NOP,         OP_NEG,
    OP_RETURN,      OP_ROT,
    OP_ROTN,        OP_NOP,
    OP_NOP,         OP_NOP,
    OP_NOP,         OP_ALLOC,
    OP_LDROP,       OP_SIZEOF,
    OP_NOP,         OP_INIT,
    OP_DROP,        OP_NOP,
    OP_NOP,         OP_EQ,

    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_CONVERT,
    OP_CONVERT,     OP_NOP,
    OP_NOP,         OP_NOP,
    OP_NOP,
};
static_assert(sizeof(tok_to_opcode)/sizeof(Opcode)-1 == TOK_LAST, "Update tok_to_opcode table in compiler");

static const char *directives[] = {
    "start", "end",
    "link",
};
static_assert(sizeof(directives)/sizeof(const char *)-1 == DIR_LAST, "Update attributes table in compiler");

static const char *attributes[] = {
    "private",
    "init", "fini",
};
static_assert(sizeof(attributes)/sizeof(const char *)-1 == ATTR_LAST, "Update attributes table in compiler");

String_View strip_file_path(const char *path) {
    String_View stripped = { .len = 0, .str = path };
    while (*path != '\0') {
        if (*path == '/' || *path == '\\')
            stripped.str = path + 1;
        path++;
    }
    stripped.len = path - stripped.str;
    while (path != stripped.str) {
        if (*path == '.')
            stripped.len = path - stripped.str;
        path--;
    }
    return stripped;
}

int validate_module_name(Compilation_Unit *compiler) {
    String_View module_name = compiler->module->name;
    if (module_name.len == 0) {
        compiler->global->had_error = 1;
        eprintf(compiler->lexer->file_path, (Loc){ 0, 0 }, LEVEL_ERR, "Module name cannot be empty\n");
        return 0;
    }

    if (isdigit(module_name.str[0])) {
        compiler->global->had_error = 1;
        eprintf(compiler->lexer->file_path, (Loc){ 0, 0 }, LEVEL_ERR, "Invalid name for module: %.*s\n", module_name.len, module_name.str);
        return 0;
    }
    for (size_t i = 0; i < module_name.len; i++) {
        if (!isalnum(module_name.str[i]) && module_name.str[i] != '_') {
            compiler->global->had_error = 1;
            eprintf(compiler->lexer->file_path, (Loc){ 0, 0 }, LEVEL_ERR, "Invalid name for module: %.*s\n", module_name.len, module_name.str);
            return 0;
        }
    }
    return 1;
}

void init_compiler(Compiler *compiler, Compiler_Options options) {
    *compiler = (Compiler){0};
    compiler->options = options;
    init_arena(&compiler->arena, 2 * 1024 * 1024);
}

Symbol *init_compilation_unit(Compilation_Unit *unit, Lexer *lexer, Compiler *global) {
    *unit = (Compilation_Unit){0};
    unit->lexer = lexer;
    unit->global = global;

    Symbol *module = arena_calloc(&global->arena, sizeof(Symbol));
    module->type = STYPE_MODULE;

    unit->module = &module->as.module;
    unit->module->name = strip_file_path(lexer->file_path);
    unit->module->path = (String_View){ .len = unit->module->name.str - lexer->file_path, .str = lexer->file_path, };
    unit->module->parent = NULL;
    unit->module->symbols = (Hashmap){0};

    return module;
}

const char *opcode_spelling(Opcode opcode) {
    return opcodes[opcode];
}

void print_op(Op *op) {
    printf("%s", opcodes[op->opcode]);

    switch (op->opcode) {
    case OP_JMPF:
    case OP_JMP:
    case OP_LABEL:
    case OP_ELSEIF:
    case OP_ELSE:
    case OP_END:
        printf(" %llu", op->operand);
        break;
    case OP_PUSH:
        printf(" %s ", type_spelling(op->types[0]));

        if (op->types[0].kind == TYPE_REAL)
            printf("%lf", *(double *)&op->operand);
        else
            printf("%llu", op->operand);
        break;
    case OP_CCALL:
    case OP_CALL:
    case OP_FUNC: {
        Hash_Entry *entry = (Hash_Entry *)op->operand;
        Function func = ((Symbol *)entry->val)->as.func;

        if (func.module_name.str == NULL)
            printf(" %.*s", entry->key_len, entry->key);
        else
            printf(" %.*s::%.*s", SV_ARG(func.module_name), entry->key_len, entry->key);
        break;
    }
    case OP_STR:
        printf(" \"%s\"", (char *)op->operand);
        break;
    case OP_CONVERT:
        printf(" (%s => %s) %llu", type_spelling(op->types[0]), type_spelling(op->types[1]), op->operand);
        break;
    case OP_STORE:
    case OP_ACCESS: {
        String_View *sv = (String_View *)op->operand;
        printf(" %.*s", SV_ARG(*sv));
        break;
    }
    case OP_PUSH_GLOBAL:
    case OP_GLOBAL: {
        Global *global = (Global *)op->operand;
        printf(" %.*s::%.*s", SV_ARG(global->module_name), SV_ARG(global->name));
        break;
    }
    case OP_CALL_MACRO: {
        Hash_Entry *entry = (Hash_Entry *)op->operand;
        printf(" %.*s", entry->key_len, entry->key);
        break;
    }
    default:
        if (op->types[1].kind != TYPE_VOID)
            printf(" %s", type_spelling(op->types[1]));
        if (op->types[0].kind != TYPE_VOID)
            printf(" %s", type_spelling(op->types[0]));
        break;
    }
    printf("\n");
}

void print_ops(Ops *ops) {
    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        printf("%s:%d:%d: ", op->file_path, op->loc.line, op->loc.pos);
        print_op(op);
    }
}

Hash_Entry *add_symbol(Compilation_Unit *compiler, Symbol *sym) {
    Hash_Entry *entry = hashmap_add(&compiler->module->symbols, compiler->lexer->prev.start, compiler->lexer->prev.len, sym);
    if (!entry) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Redefinition of a symbol\n");
        return hashmap_get(&compiler->module->symbols, compiler->lexer->prev.start, compiler->lexer->prev.len);
    }
    return entry;
}

static Op *make_op(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .loc = compiler->lexer->prev.loc,
    };
    DA_APPEND(&compiler->ops, op);
    return &compiler->ops.items[compiler->ops.count-1];
}

Op *make_op_at_cur(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .loc = compiler->lexer->cur.loc,
    };
    DA_APPEND(&compiler->ops, op);
    return &compiler->ops.items[compiler->ops.count-1];
}

Unresolved_Symbol *make_unresolved(Compilation_Unit *compiler, Unresolved_Type type) {
    Unresolved_Symbol sym = {
        .name = { .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start },
        .loc = compiler->lexer->prev.loc,
        .type = type,
    };
    DA_APPEND(&compiler->unresolved, sym);
    return &compiler->unresolved.items[compiler->unresolved.count-1];
}

static int expect(Compilation_Unit *compiler, Token_Type type) {
    lexer_next(compiler->lexer);
    if (compiler->lexer->prev.type == TOK_ERROR) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", compiler->lexer->prev.len, compiler->lexer->prev.start);
    }
    if (compiler->lexer->prev.type != type) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", tok_spelling(type), tok_spelling(compiler->lexer->prev.type));
        return 0;
    }
    return 1;
}

void compile_stmt(Compilation_Unit *compiler);

void compile_if_stmt(Compilation_Unit *compiler) {
    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_THEN && cur->type != TOK_FUNC && cur->type != TOK_EOF)
        compile_stmt(compiler);
    if (!expect(compiler, TOK_THEN)) return;

    int if_start = compiler->ops.count;
    make_op(compiler, OP_IF, 0);

    while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_ELSEIF && cur->type != TOK_ELSE &&
           cur->type != TOK_FUNC && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[if_start].operand = compiler->label_count;

    Dyn_Backpatchees elseifs = {0};

    while (cur->type == TOK_ELSEIF) {
        lexer_next(compiler->lexer);

        DA_APPEND(&elseifs, compiler->ops.count);
        make_op(compiler, OP_JMP, 0);
        make_op_at_cur(compiler, OP_ELSEIF, compiler->label_count++);

        while (cur->type != TOK_THEN && cur->type != TOK_FUNC && cur->type != TOK_EOF)
            compile_stmt(compiler);
        lexer_next(compiler->lexer);

        int branch_start = compiler->ops.count;
        make_op(compiler, OP_JMPF, 0);

        while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_ELSEIF && cur->type != TOK_ELSE &&
               cur->type != TOK_FUNC && cur->type != TOK_EOF)
            compile_stmt(compiler);
        compiler->ops.items[branch_start].operand = compiler->label_count;
    }

    if (cur->type == TOK_ELSE) {
        lexer_next(compiler->lexer);

        int else_start = compiler->ops.count;
        make_op(compiler, OP_JMP, 0);
        make_op_at_cur(compiler, OP_ELSE, compiler->label_count++);

        while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_FUNC && cur->type != TOK_EOF)
            compile_stmt(compiler);
        compiler->ops.items[else_start].operand = compiler->label_count;
    }

    if (cur->type == TOK_EOF || cur->type == TOK_FUNC) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of if statement.\n");
        return;
    }

    for (size_t i = 0; i < elseifs.count; i++)
        compiler->ops.items[elseifs.items[i]].operand = compiler->label_count;

    if (elseifs.count > 0)
        free(elseifs.items);

    lexer_next(compiler->lexer);
    make_op(compiler, OP_END, compiler->label_count++);
}

void compile_while_stmt(Compilation_Unit *compiler) {
    compiler->is_in_loop = 1;

    int loop_label = compiler->label_count;
    make_op(compiler, OP_START, compiler->label_count++);

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_LOOP && cur->type != TOK_LBRACE && cur->type != TOK_FUNC && cur->type != TOK_EOF)
        compile_stmt(compiler);

    int loop_start = compiler->ops.count;
    make_op_at_cur(compiler, OP_JMPF, 0);
    make_op_at_cur(compiler, OP_END, compiler->label_count++);

    Token_Type end_type = cur->type == TOK_LBRACE ? TOK_RBRACE : TOK_END;

    if (cur->type == TOK_EOF || cur->type == TOK_FUNC) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected start of loop\n");
        return;
    }

    lexer_next(compiler->lexer);
    make_op(compiler, OP_START, UINT64_MAX);

    while (cur->type != TOK_RBRACE && cur->type != TOK_END && cur->type != TOK_FUNC && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[loop_start].operand = compiler->label_count;

    if (!expect(compiler, end_type)) return;

    make_op(compiler, OP_JMP, loop_label);

    for (int i = compiler->brks.count-1; i >= 0; i--) {
        if (compiler->brks.positions[i] < loop_start) break;
        compiler->ops.items[compiler->brks.positions[i]].operand = compiler->label_count;
        compiler->brks.count--;
    }
    for (int i = compiler->conts.count-1; i >= 0; i--) {
        if (compiler->conts.positions[i] < loop_start) break;
        compiler->ops.items[compiler->conts.positions[i]].operand = loop_label;
        compiler->conts.count--;
    }
    make_op(compiler, OP_END, compiler->label_count++);

    compiler->is_in_loop = 0;
}

void compile_loop_stmt(Compilation_Unit *compiler) {
    compiler->is_in_loop = 1;

    Token_Type end_type = compiler->lexer->prev.type == TOK_LBRACE ? TOK_RBRACE : TOK_END;

    int loop_label = compiler->label_count;
    make_op(compiler, OP_START, compiler->label_count++);
    int loop_start = compiler->ops.count;

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_RBRACE && cur->type != TOK_FUNC && cur->type != TOK_END)
        compile_stmt(compiler);
    if (!expect(compiler, end_type)) return;

    make_op(compiler, OP_JMP, loop_label);

    for (int i = compiler->brks.count-1; i >= 0; i--) {
        if (compiler->brks.positions[i] < loop_start) break;
        compiler->ops.items[compiler->brks.positions[i]].operand = compiler->label_count;
        compiler->brks.count--;
    }
    for (int i = compiler->conts.count-1; i >= 0; i--) {
        if (compiler->conts.positions[i] < loop_start) break;
        compiler->ops.items[compiler->conts.positions[i]].operand = loop_label;
        compiler->conts.count--;
    }
    make_op(compiler, OP_END, compiler->label_count++);

    compiler->is_in_loop = 0;
}

void compile_grouping(Compilation_Unit *compiler) {
    while (compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_FUNC && compiler->lexer->cur.type != TOK_EOF)
        compile_stmt(compiler);
    expect(compiler, TOK_RPAREN);
}

void compile_struct_fields(Compilation_Unit *compiler, Struct *structure);

Advanced_Type *compile_anonymous_struct(Compilation_Unit *compiler) {
    Advanced_Type type = {0};
    compile_struct_fields(compiler, &type.structure);
    DA_APPEND(&compiler->types, type);
    return &compiler->types.items[compiler->types.count-1];
}

Hash_Entry *get_entry_in_module(Compilation_Unit *compiler) {
    Token *prev = &compiler->lexer->prev;
    if (prev->type != TOK_WORD) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected word. Please report this as a bug");
        return NULL;
    }

    String_View full_name = { .str = prev->start, .len = 0 };
    while (compiler->lexer->cur.type == TOK_SCOPE) {
        full_name.len = compiler->lexer->cur.start - full_name.str;
        lexer_next(compiler->lexer);
        if (!expect(compiler, TOK_WORD)) return NULL;
    }

    Hash_Entry *module_entry = hashmap_get(&compiler->module->symbols, full_name.str, full_name.len);
    if (!module_entry || !module_entry->key) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Unknown module %.*s\n", SV_ARG(full_name));
        return NULL;
    }

    Symbol *module = (Symbol *)module_entry->val;
    Hash_Entry *entry = hashmap_get(&module->as.module.symbols, prev->start, prev->len);
    if (!entry || !entry->key) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Unknown symbol %.*s in module %.*s\n", prev->len, prev->start, SV_ARG(full_name));
        return NULL;
    }

    Symbol *sym = (Symbol *)entry->val;
    if (sym->attributes.private) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Private symbol %.*s in module %.*s\n", prev->len, prev->start, SV_ARG(full_name));
        return NULL;
    }
    return entry;
}

void get_type(Compilation_Unit *compiler, Type *type) {
    Token *prev = &compiler->lexer->prev;
    switch (prev->type) {
    case TOK_I8:   *type = BASIC_TYPE(TYPE_I8);             break;
    case TOK_CHAR: *type = BASIC_TYPE(TYPE_CHAR);           break;
    case TOK_U8:   *type = BASIC_TYPE(TYPE_U8);             break;
    case TOK_I16:  *type = BASIC_TYPE(TYPE_I16);            break;
    case TOK_U16:  *type = BASIC_TYPE(TYPE_U16);            break;
    case TOK_I32:  *type = BASIC_TYPE(TYPE_I32);            break;
    case TOK_U32:  *type = BASIC_TYPE(TYPE_U32);            break;
    case TOK_I64:  *type = BASIC_TYPE(TYPE_I64);            break;
    case TOK_U64:  *type = BASIC_TYPE(TYPE_U64);            break;
    case TOK_F32:  *type = BASIC_TYPE(TYPE_F32);            break;
    case TOK_F64:  *type = BASIC_TYPE(TYPE_F64);            break;
    case TOK_STR:  *type = PTR_TYPE(BASIC_TYPE(TYPE_CHAR)); break;
    case TOK_WORD: {
        Hash_Entry *entry = hashmap_get(&compiler->module->symbols, prev->start, prev->len);
        if (!entry || !entry->key) {
            Unresolved_Symbol *unresolved = make_unresolved(compiler, UTYPE_TYPE);
            unresolved->as.type = type;
            break;
        }
        if (compiler->lexer->cur.type == TOK_SCOPE) {
            entry = get_entry_in_module(compiler);
            if (!entry) break;
        }

        Symbol *sym = (Symbol *)entry->val;
        if (sym->type != STYPE_TYPE) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "%.*s is not a structure\n", entry->key_len, entry->key);
            *type = BASIC_TYPE(TYPE_VOID);
            break;
        }

        *type = ADVANCED_TYPE(TYPE_STRUCT, sym->as.type);
        break;
    }
    case TOK_STRUCT:
        *type = ADVANCED_TYPE(TYPE_STRUCT, compile_anonymous_struct(compiler));
        type->advanced->kind = TYPE_STRUCT;
        break;
    case TOK_UNION:
        *type = ADVANCED_TYPE(TYPE_UNION, compile_anonymous_struct(compiler));
        type->advanced->kind = TYPE_UNION;
        break;
    case TOK_PTR: {
        expect(compiler, TOK_LBRACKET);

        lexer_next(compiler->lexer);
        get_type(compiler, type);
        *type = PTR_TYPE(*type);

        expect(compiler, TOK_RBRACKET);
        break;
    }
    default:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected type, got %s\n", tok_spelling(prev->type));
        *type = BASIC_TYPE(TYPE_VOID);
        break;
    }
}

void compile_entry(Compilation_Unit *compiler, Hash_Entry *entry) {
    if (compiler->lexer->cur.type == TOK_SCOPE) {
        entry = get_entry_in_module(compiler);
        if (!entry) return;
    }
    else if (!entry || !entry->key) {
        Unresolved_Symbol *unresolved = make_unresolved(compiler, UTYPE_OP);
        unresolved->as.op = compiler->ops.count;
        make_op(compiler, OP_UNKNOWN, 0);
        return;
    }

    Symbol *sym = (Symbol *)entry->val;
    switch (sym->type) {
    case STYPE_FUNC:
        if (sym->as.func.is_c_func)
            make_op(compiler, OP_CCALL, (int64_t)entry);
        else
            make_op(compiler, OP_CALL, (int64_t)entry);
        break;
    case STYPE_TYPE: {
        Op *op = make_op(compiler, OP_INIT, 0);
        op->types[0] = ADVANCED_TYPE(sym->as.type->kind, sym->as.type);
        break;
    }
    case STYPE_CONST: {
        Op *op = make_op(compiler, sym->as.constant.type.ptr_depth > 0 ? OP_STR : OP_PUSH, sym->as.constant.val);
        op->types[0] = sym->as.constant.type;
        break;
    }
    case STYPE_GLOBAL: {
        Op *op = make_op(compiler, OP_PUSH_GLOBAL, (int64_t)&sym->as.global);
        op->types[0] = sym->as.global.type;
        break;
    }
    case STYPE_MACRO: {
        make_op(compiler, OP_CALL_MACRO, (int64_t)entry);
        break;
    }
    case STYPE_MODULE:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected scope symbol after module name\n");
        break;
    }
}

void compile_stmt(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);

    Token *tok = &compiler->lexer->prev;
    if (tok_to_opcode[tok->type] != OP_NOP)
        make_op(compiler, tok_to_opcode[tok->type], 0);
    Op *op = &compiler->ops.items[compiler->ops.count-1];

    switch (tok->type) {
    case TOK_INT_LIT:
        op->operand = tok->as.integer;
        op->types[0] = BASIC_TYPE(TYPE_INT);
        break;
    case TOK_REAL_LIT:
        op->operand = tok->as.integer;
        op->types[0] = BASIC_TYPE(TYPE_REAL);
        break;
    case TOK_CHAR_LIT:
        op->operand = tok->as.integer;
        op->types[0] = BASIC_TYPE(TYPE_CHAR);
        break;
    case TOK_STR_LIT:
        op->operand = (int64_t)tok->as.str;
        break;
    case TOK_IF:
        compile_if_stmt(compiler);
        break;
    case TOK_WHILE:
        compile_while_stmt(compiler);
        break;
    case TOK_LBRACE:
    case TOK_LOOP:
        compile_loop_stmt(compiler);
        break;
    case TOK_LPAREN:
        compile_grouping(compiler);
        break;
    case TOK_BRK:
        if (!compiler->is_in_loop) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Break can only be in loops\n");
            return;
        }
        compiler->brks.positions[compiler->brks.count++] = compiler->ops.count;
        op->operand = -1;
        break;
    case TOK_CONTINUE:
        if (!compiler->is_in_loop) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Continue can only be in loops\n");
            return;
        }
        compiler->conts.positions[compiler->conts.count++] = compiler->ops.count;
        op->operand = -1;
        break;
    case TOK_RET:
        compiler->rets.positions[compiler->rets.count++] = compiler->ops.count;
        op->operand = -1;
        break;
    case TOK_WORD: {
        Hash_Entry *entry = hashmap_get(&compiler->module->symbols, tok->start, tok->len);
        compile_entry(compiler, entry);
        break;
    }
    case TOK_STRUCT:
        op->types[0].kind = TYPE_STRUCT;
        op->types[0].advanced = compile_anonymous_struct(compiler);
        op->types[0].advanced->kind = TYPE_STRUCT;
        break;
    case TOK_UNION:
        op->types[0].kind = TYPE_UNION;
        op->types[0].advanced = compile_anonymous_struct(compiler);
        op->types[0].advanced->kind = TYPE_UNION;
        break;
    case TOK_I8:
    case TOK_U8:
    case TOK_CHAR:
    case TOK_I16:
    case TOK_U16:
    case TOK_I32:
    case TOK_U32:
    case TOK_I64:
    case TOK_U64:
    case TOK_F32:
    case TOK_F64:
    case TOK_PTR:
    case TOK_STR:
        get_type(compiler, &op->types[1]);
        break;
    case TOK_HASH:
    case TOK_ARROW_HASH:
    case TOK_DOT: {
        if (!expect(compiler, TOK_WORD)) return;
        String_View *sv = arena_calloc(&compiler->global->arena, sizeof(String_View));
        *sv = (String_View){ .len = tok->len, .str = tok->start };
        op->operand = (int64_t)sv;
        break;
    }
    case TOK_ALLOC:
        lexer_next(compiler->lexer);
        get_type(compiler, &op->types[0]);
        break;
    case TOK_SIZEOF:
        lexer_next(compiler->lexer);
        get_type(compiler, &op->types[0]);
        break;
    case TOK_ERROR:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", tok->len, tok->start);
        return;
    case TOK_COLON:
    case TOK_SEMICOLON:
    case TOK_ELSE:
    case TOK_END:
    case TOK_RBRACE:
    case TOK_RPAREN:
    case TOK_LBRACKET:
    case TOK_RBRACKET:
    case TOK_SCOPE:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Lone %s\n", tok_spelling(tok->type));
        return;
    case TOK_IMPORT:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Import is not allowed here\n");
        return;
    case TOK_FUNC:
    case TOK_EXT_FUNC:
    case TOK_EOF:
        eprintf(__FILE__, (Loc){ .line = __LINE__, .pos = -1 }, LEVEL_ERR, "Invalid token %s reached in compile_stmt\n", tok_spelling(tok->type));
        exit(1);
    default: break;
    }
}

void compile_signature(Compilation_Unit *compiler, Types *param_types, Types *return_types) {
    if (compiler->lexer->cur.type == TOK_EOF) return;
    expect(compiler, TOK_LPAREN);

    while (compiler->lexer->cur.type != TOK_ARROW && compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(param_types, (Type){0});
        get_type(compiler, &param_types->items[param_types->count-1]);
    }

    if (compiler->lexer->cur.type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected arrow or right parenthesis, got end of file\n");
        return;
    }
    lexer_next(compiler->lexer);

    if (compiler->lexer->prev.type == TOK_RPAREN) return;

    while (compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(return_types, (Type){0});
        get_type(compiler, &return_types->items[return_types->count-1]);
    }

    if (compiler->lexer->cur.type == TOK_EOF) return;
    expect(compiler, TOK_RPAREN);
}

Hash_Entry *compile_function_signature(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);
    expect(compiler, TOK_WORD);
    if (compiler->lexer->prev.type == TOK_EOF) return NULL;

    Symbol *sym = arena_calloc(&compiler->global->arena, sizeof(Symbol));
    sym->type = STYPE_FUNC;
    sym->attributes = (Attributes){ .value = compiler->global_attrs.value | compiler->sym_attrs.value };
    sym->as.func.module_name = compiler->module->full_name;

    Hash_Entry *entry = add_symbol(compiler, sym);
    compile_signature(compiler, &sym->as.func.param_types, &sym->as.func.return_types);

    return entry;
}

void compile_function(Compilation_Unit *compiler) {
    Op *op = make_op(compiler, OP_FUNC, 0);

    Hash_Entry *entry = compile_function_signature(compiler);
    if (!entry) return;

    op->operand = (int64_t)entry;

    Function *func = &((Symbol *)entry->val)->as.func;
    func->extern_name = (String_View){ .len = entry->key_len, .str = entry->key };

    while (compiler->lexer->cur.type != TOK_FUNC && compiler->lexer->cur.type != TOK_EOF)
        compile_stmt(compiler);

    lexer_next(compiler->lexer);
    for (int i = compiler->rets.count-1; i >= 0; i--) {
        compiler->ops.items[compiler->rets.positions[i]].operand = compiler->label_count;
        compiler->rets.count--;
    }

    make_op(compiler, OP_LABEL, compiler->label_count++);
    make_op(compiler, OP_RET, 0);

    if (compiler->lexer->prev.type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of function, got end of file\n");
    }
}

void compile_external_function(Compilation_Unit *compiler, uint8_t is_c_func) {
    Hash_Entry *entry = compile_function_signature(compiler);
    Function *func = &((Symbol *)entry->val)->as.func;
    func->is_c_func = is_c_func;

    if (compiler->lexer->cur.type == TOK_EQ) {
        lexer_next(compiler->lexer);
        if (!expect(compiler, TOK_STR_LIT)) return;
        func->extern_name = (String_View){ .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start, };
    }
    else {
        func->extern_name = (String_View){ .len = entry->key_len, .str = entry->key };
    }
}

void compile_struct_fields(Compilation_Unit *compiler, Struct *structure) {
    while (compiler->lexer->cur.type != TOK_END && compiler->lexer->cur.type != TOK_EOF) {
        expect(compiler, TOK_WORD);
        if (compiler->lexer->prev.type == TOK_EOF) return;

        Field field = { .name = { .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start }, };

        expect(compiler, TOK_COLON);
        if (compiler->lexer->prev.type == TOK_EOF) return;

        lexer_next(compiler->lexer);
        ARENA_DA_APPEND(&compiler->global->arena, &structure->fields, field);

        get_type(compiler, &structure->fields.items[structure->fields.count-1].type);
    }
    expect(compiler, TOK_END);
}

void compile_struct(Compilation_Unit *compiler, Type_Kind type) {
    Symbol *sym;
    Attributes valid = { .private = 1 };
    START_DECL(STYPE_TYPE);

    DA_APPEND(&compiler->types, (Advanced_Type){0});
    sym->as.type = &compiler->types.items[compiler->types.count-1];
    sym->as.type->loc = compiler->lexer->prev.loc;

    sym->as.type->kind = type;
    sym->as.type->structure.name = (String_View){ .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start };

    compile_struct_fields(compiler, &sym->as.type->structure);
}

void compile_const(Compilation_Unit *compiler) {
    Symbol *sym;
    Attributes valid = { .private = 1 };
    START_DECL(STYPE_CONST);

    lexer_next(compiler->lexer);
    switch (compiler->lexer->prev.type) {
    case TOK_INT_LIT:
        sym->as.constant.type = BASIC_TYPE(TYPE_INT);
        sym->as.constant.val = compiler->lexer->prev.as.integer;
        break;
    case TOK_REAL_LIT:
        sym->as.constant.type = BASIC_TYPE(TYPE_REAL);
        sym->as.constant.val = compiler->lexer->prev.as.integer;
        break;
    case TOK_CHAR_LIT:
        sym->as.constant.type = BASIC_TYPE(TYPE_CHAR);
        sym->as.constant.val = compiler->lexer->prev.as.integer;
        break;
    case TOK_STR_LIT:
        sym->as.constant.type = PTR_TYPE(BASIC_TYPE(TYPE_CHAR));
        sym->as.constant.val = (uint64_t)compiler->lexer->prev.as.str;
        break;
    default:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Constants cannot be set to non-literals\n");
        break;
    }
}

void compile_global(Compilation_Unit *compiler) {
    Symbol *sym;
    Attributes valid = { .private = 1 };
    START_DECL(STYPE_GLOBAL);

    sym->as.global.name = (String_View){ .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start };
    sym->as.global.module_name = compiler->module->full_name;

    make_op(compiler, OP_GLOBAL, (int64_t)&sym->as.global);

    expect(compiler, TOK_COLON);
    if (compiler->lexer->prev.type == TOK_EOF) return;

    lexer_next(compiler->lexer);
    get_type(compiler, &sym->as.global.type);
}

void compile_macro(Compilation_Unit *compiler) {
    Symbol *sym;
    Attributes valid = { .private = 1 };
    START_DECL(STYPE_MACRO);

    compile_signature(compiler, &sym->as.func.param_types, &sym->as.func.return_types);

    make_op(compiler, OP_MACRO, 0);
    int macro_start = compiler->ops.count;

    sym->as.func.ops.items = compiler->ops.items + compiler->ops.count;
    while (compiler->lexer->cur.type != TOK_END && compiler->lexer->cur.type != TOK_EOF)
        compile_stmt(compiler);
    sym->as.func.ops.count = compiler->ops.count - macro_start;

    lexer_next(compiler->lexer);
    make_op(compiler, OP_END, 0);

    if (compiler->lexer->prev.type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of function, got end of file\n");
    }
}

void compile_attributes(Compilation_Unit *compiler, Attributes *attrs) {
    Token *prev = &compiler->lexer->prev;

    expect(compiler, TOK_LPAREN);
    while (compiler->lexer->cur.type == TOK_WORD) {
        lexer_next(compiler->lexer);

        Attribute_Type i;
        for (i = 0; i <= ATTR_LAST; i++)
            if (strncmp(attributes[i], prev->start, prev->len) == 0)
                break;
        if (i == ATTR_LAST+1) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "%.*s is not an attribute\n", prev->len, prev->start);
            break;
        }

        switch (i) {
        case ATTR_PRIVATE:
            if (attrs->private)
                COMPILER_EPRINTF(LEVEL_WARN, "Symbol is already private\n");
            attrs->private = 1;
            break;
        case ATTR_INIT:
            if (attrs->init)
                COMPILER_EPRINTF(LEVEL_WARN, "Function is already an init function\n");
            attrs->init = 1;
            break;
        case ATTR_FINI:
            if (attrs->fini)
                COMPILER_EPRINTF(LEVEL_WARN, "Function is already a final function\n");
            attrs->fini = 1;
            break;
        }
    }
    expect(compiler, TOK_RPAREN);
}

void compile_directive(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);
    Token *prev = &compiler->lexer->prev;
    if (compiler->lexer->cur.type == TOK_LPAREN) {
        compile_attributes(compiler, &compiler->sym_attrs);
        return;
    }
    expect(compiler, TOK_WORD);

    Directive_Type i;
    for (i = 0; i <= DIR_LAST; i++)
        if (strncmp(directives[i], prev->start, prev->len) == 0)
            break;
    if (i == DIR_LAST+1) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s is not a directive\n", prev->len, prev->start);
        return;
    }

    switch (i) {
    case DIR_START:
        compile_attributes(compiler, &compiler->global_attrs);
        break;
    case DIR_END: {
        Attributes end = {0};
        compile_attributes(compiler, &end);
        if (~compiler->global_attrs.value & end.value) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Can't end attributes that haven't been started\n");
        }
        compiler->global_attrs.value &= ~end.value;
        break;
    }
    case DIR_LINK: {
        expect(compiler, TOK_LPAREN);
        expect(compiler, TOK_STR_LIT);

        size_t link_path_len = compiler->module->path.len + strlen(prev->as.str);
        char *link_path = arena_calloc(&compiler->global->arena, link_path_len+1);
        snprintf(link_path, link_path_len+1, "%.*s%s", SV_ARG(compiler->module->path), prev->as.str);

        DA_APPEND(&compiler->global->options.link_cmd, link_path);

        expect(compiler, TOK_RPAREN);
        return;
    }
    }
}

void compile_decls(Compilation_Unit *compiler) {
    for (; ;) {
        if (compiler->lexer->prev.type == TOK_EOF) break;

        switch (compiler->lexer->cur.type) {
        case TOK_FUNC:
            compile_function(compiler);
            continue;
        case TOK_EXT_FUNC:
            compile_external_function(compiler, 0);
            continue;
        case TOK_C_FUNC:
            compile_external_function(compiler, 1);
            continue;
        case TOK_STRUCT:
            compile_struct(compiler, TYPE_STRUCT);
            continue;
        case TOK_UNION:
            compile_struct(compiler, TYPE_UNION);
            continue;
        case TOK_CONST:
            compile_const(compiler);
            continue;
        case TOK_GLOBAL:
            compile_global(compiler);
            continue;
        case TOK_MACRO:
            compile_macro(compiler);
            continue;
        case TOK_AT:
            compile_directive(compiler);
            continue;
        default: break;
        }
        break;
    }

    Hash_Entry *main = hashmap_get(&compiler->module->symbols, "main", 4);
    if (main && main->val) {
        Symbol *main_sym = (Symbol *)main->val;
        if (main_sym->type != STYPE_FUNC) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "main can only be a function\n");
        }
        else {
            main_sym->as.func.module_name = (String_View){0};
        }
    }

    if (compiler->lexer->prev.type == TOK_EOF) return;
    if (compiler->lexer->cur.type != TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Extra tokens at end of file\n");
    }
}

int resolve_type(Compilation_Unit *compiler, Advanced_Type *type) {
    if (type->resolve_status == STATUS_RESOLVED)
        return 1;

    if (type->resolve_status == STATUS_RESOLVING) {
        compiler->global->had_error = 1;
        eprintf(compiler->lexer->file_path, type->loc, LEVEL_ERR, "Type is recursive\n");
        return 0;
    }
    type->resolve_status = STATUS_RESOLVING;

    int offset = 0;
    int largest_size = 0;
    int alignment = 0;

    for (size_t i = 0; i < type->structure.fields.count; i++) {
        Field *field = &type->structure.fields.items[i];
        if (IS_ADVANCED(field->type) && !resolve_type(compiler, field->type.advanced))
            return 0;

        int field_alignment = type_alignment(field->type);
        if (field_alignment > alignment)
            alignment = field_alignment;

        int size = type_size(field->type);
        if (size > largest_size)
            largest_size = size;

        offset = ALIGN(offset, field_alignment);
        field->offset = offset;

        if (type->kind == TYPE_STRUCT)
            offset += size;
    }

    type->structure.alignment = alignment;
    type->structure.size = MAX(offset, largest_size);
    type->resolve_status = STATUS_RESOLVED;

    return 1;
}

void resolve_types(Compilation_Unit *compiler) {
    for (size_t i = 0; i < compiler->types.count; i++) {
        Advanced_Type *type = &compiler->types.items[i];
        resolve_type(compiler, type);
    }
}

void resolve_symbols(Compilation_Unit *compiler) {
    for (size_t i = 0; i < compiler->unresolved.count; i++) {
        Unresolved_Symbol unresolved = compiler->unresolved.items[i];

        Hash_Entry *entry = hashmap_get(&compiler->module->symbols, unresolved.name.str, unresolved.name.len);
        if (!entry || !entry->key) {
            compiler->global->had_error = 1;
            eprintf(compiler->lexer->file_path, unresolved.loc, LEVEL_ERR, "Unknown symbol %.*s\n", SV_ARG(unresolved.name));
            continue;
        }

        Symbol *sym = (Symbol *)entry->val;

        switch (unresolved.type) {
        case UTYPE_OP: {
            Op *op = &compiler->ops.items[unresolved.as.op];
            switch (sym->type) {
            case STYPE_FUNC:
                op->opcode = sym->as.func.is_c_func ? OP_CCALL : OP_CALL;
                op->operand = (uint64_t)entry;
                break;
            case STYPE_TYPE:
                op->opcode = OP_INIT;
                op->types[0] = ADVANCED_TYPE(TYPE_STRUCT, sym->as.type);
                break;
            case STYPE_CONST:
                op->opcode = sym->as.constant.type.ptr_depth > 0 ? OP_STR : OP_PUSH;
                op->operand = sym->as.constant.val;
                op->types[0] = sym->as.constant.type;
                break;
            case STYPE_MODULE:
                compiler->global->had_error = 1;
                COMPILER_EPRINTF(LEVEL_ERR, "Unreachable. Please report this as a bug.");
                break;
            case STYPE_GLOBAL:
                op->opcode = OP_PUSH_GLOBAL;
                op->operand = (uint64_t)entry;
                op->types[0] = sym->as.global.type;
                break;
            case STYPE_MACRO:
                op->opcode = OP_CALL_MACRO;
                op->operand = (uint64_t)entry;
                break;
            }
            break;
        }
        case UTYPE_TYPE: {
            if (sym->type != STYPE_TYPE) {
                compiler->global->had_error = 1;
                eprintf(compiler->lexer->file_path, unresolved.loc, LEVEL_ERR, "%.*s is not a structure\n",
                        entry->key_len, entry->key);
                break;
            }
            *unresolved.as.type = ADVANCED_TYPE(TYPE_STRUCT, sym->as.type);
            break;
        }
        }
    }
}

void compile_module_name(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);
    expect(compiler, TOK_WORD);

    Token *prev = &compiler->lexer->prev;
    const char *full_name = prev->start;

    while (compiler->lexer->cur.type == TOK_SCOPE) {
        Hashmap *parent_symbols = compiler->module->parent == NULL ? &compiler->global->modules : &compiler->module->parent->symbols;
        Hash_Entry *entry = hashmap_get(parent_symbols, prev->start, prev->len);
        if (!entry || !entry->key) {
            Symbol *module = arena_calloc(&compiler->global->arena, sizeof(Symbol));
            module->type = STYPE_MODULE;
            module->as.module.parent = compiler->module->parent;
            module->as.module.status = STATUS_RESOLVED;

            hashmap_add(parent_symbols, prev->start, prev->len, module);
        }
        compiler->module->parent = &((Symbol *)entry->val)->as.module;
        lexer_next(compiler->lexer);
        expect(compiler, TOK_WORD);
    }

    compiler->module->full_name = (String_View){ .len = (prev->start + prev->len) - full_name, .str = full_name };
    compiler->module->name = (String_View){ .len = prev->len, .str = prev->start };
}

Symbol *get_module_in_module(Compilation_Unit *compiler, Hashmap **parent) {
    Token *prev = &compiler->lexer->prev;
    if (prev->type != TOK_WORD) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected word. Please report this as a bug\n");
        return NULL;
    }

    Hash_Entry *entry;
    Symbol *sym;
    for (; ;) {
        entry = hashmap_get(*parent, prev->start, prev->len);
        if (!entry || !entry->key)
            return NULL;

        sym = (Symbol *)entry->val;
        if (sym->type != STYPE_MODULE) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Not a module\n");
            return sym;
        }
        if (compiler->lexer->cur.type != TOK_SCOPE) break;

        *parent = &sym->as.module.symbols;

        lexer_next(compiler->lexer);
        expect(compiler, TOK_WORD);
    }
    return sym;
}

Symbol *compile_module(Compiler *global, char *src, const char *file_path);

void resolve_imports(Compilation_Unit *compiler) {
    while (compiler->lexer->cur.type == TOK_IMPORT) {
        lexer_next(compiler->lexer);
        expect(compiler, TOK_WORD);

        if (compiler->lexer->prev.len == 3 && strncmp(compiler->lexer->prev.start, "std", 3) == 0) {
            if (compiler->lexer->cur.type != TOK_SCOPE) {
                compiler->global->had_error = 1;
                COMPILER_EPRINTF(LEVEL_ERR, "Cannot import std. Please specify a module in std\n");
                continue;
            }
            lexer_next(compiler->lexer);
            expect(compiler, TOK_WORD);

            size_t path_len = compiler->lexer->prev.len + compiler->global->options.compiler_dir.len + 11;
            char *path = arena_calloc(&compiler->global->arena, path_len);
            snprintf(path, path_len, "%.*s/std/%.*s.alng", SV_ARG(compiler->global->options.compiler_dir), compiler->lexer->prev.len, compiler->lexer->prev.start);

            char *contents = open_file(path);
            if (!contents) {
                COMPILER_EPRINTF(LEVEL_ERR, "Could not read file %s: %s\n", path, strerror(errno));
                exit(1);
            }

            Symbol *sym = compile_module(compiler->global, contents, path);
            hashmap_add(&compiler->module->symbols, sym->as.module.full_name.str, sym->as.module.full_name.len, sym);
            continue;
        }

        Hashmap *module_symbols = &compiler->global->modules;
        Symbol *module = get_module_in_module(compiler, &module_symbols);
        if (module && module->as.module.status == STATUS_UNRESOLVED) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Module is recursive\n");
            continue;
        }

        while (!module) {
            compiler->global->file++;
            if (compiler->global->file >= compiler->global->options.input_file_count) {
                compiler->global->had_error = 1;
                COMPILER_EPRINTF(LEVEL_ERR, "Module does not exist\n");
                break;
            }

            char *input_file_path = compiler->global->options.input_files[compiler->global->file];

            char *contents = open_file(input_file_path);
            if (!contents) {
                COMPILER_EPRINTF(LEVEL_ERR, "Could not read file %s: %s\n", input_file_path, strerror(errno));
                exit(1);
            }
            compile_module(compiler->global, contents, input_file_path);

            module = get_module_in_module(compiler, &module_symbols);
        }
        if (!module) continue;
        hashmap_add(&compiler->module->symbols, module->as.module.full_name.str, module->as.module.full_name.len, module);
    }
}

Symbol *compile_module(Compiler *global, char *src, const char *file_path) {
    Lexer lexer;
    init_lexer(&lexer, src, file_path);

    Compilation_Unit unit;
    Symbol *module_sym = init_compilation_unit(&unit, &lexer, global);

    char *obj_name = arena_calloc(&global->arena, unit.module->name.len + 3);
    snprintf(obj_name, unit.module->name.len + 3, "%.*s.o", SV_ARG(unit.module->name));

    lexer_next(&lexer);
    if (lexer.cur.type == TOK_EOF)
        eprintf(lexer.file_path, lexer.cur.loc, LEVEL_WARN, "Empty file\n");
    else if (lexer.cur.type == TOK_MODULE)
        compile_module_name(&unit);
    else if (!validate_module_name(&unit))
        return NULL;

    Hashmap *parent_symbols = unit.module->parent == NULL ? &global->modules : &unit.module->parent->symbols;
    Hash_Entry *entry = hashmap_get(parent_symbols, unit.module->name.str, unit.module->name.len);
    if (entry && entry->key) {
        global->had_error = 1;
        eprintf(file_path, lexer.prev.loc, LEVEL_ERR, "A module with that name already exists\n");
        return (Symbol *)entry->val;
    }

    DA_APPEND(&global->options.link_cmd, obj_name);
    if (!global->options.emit_obj)
        DA_APPEND(&global->cleanup, obj_name);

    entry = hashmap_add(parent_symbols, unit.module->name.str, unit.module->name.len, module_sym);
    unit.module->status = STATUS_UNRESOLVED;
    resolve_imports(&unit);

    compile_decls(&unit);

    resolve_symbols(&unit);
    resolve_types(&unit);

    unit.module->status = STATUS_RESOLVED;

    if (!global->had_error && !global->options.debug)
        global->had_error = type_check(&unit.ops);

    if (!global->had_error && !global->options.print_ir) {
        Hash_Entry *main = hashmap_get(&unit.module->symbols, "main", 4);
        char *output_asm = generate_x86_64(&unit.ops, obj_name, main != NULL && main->key != NULL);
        if (!output_asm) global->had_error = 1;

        DA_APPEND(&global->cleanup, output_asm);

        if (!global->had_error && !global->options.emit_asm) {
            Cmd cmd = {0};
            cmd_append_many(&cmd, 4, "as", "-o", obj_name, output_asm);
            cmd_exec(&cmd, global->options.verbose);
        }
    }

    if (global->options.debug || global->options.print_ir)
        print_ops(&unit.ops);

    free(unit.ops.items);
    return module_sym;
}

void link_files(Compiler_Options options) {
    Cmd cmd = {0};
    cmd_append_many(&cmd, 2, "ld", "-L/usr/lib");
    memcpy(cmd.items + cmd.count, options.link_cmd.items, options.link_cmd.count * sizeof(char *));
    cmd.count += options.link_cmd.count;
    cmd_append_many(&cmd, 4, "-dynamic-linker", "/lib64/ld-linux-x86-64.so.2", "-o", options.output_file);
    cmd_exec(&cmd, options.verbose);
}

void clean_files(Compiler *compiler) {
    Cmd cmd = {0};
    cmd_append_many(&cmd, 2, "rm", "-f");
    memcpy(cmd.items + cmd.count, compiler->cleanup.items, compiler->cleanup.count * sizeof(char *));
    cmd.count += compiler->cleanup.count;
    cmd_exec(&cmd, compiler->options.verbose);
}

void compile(Compiler_Options options) {
    Compiler compiler;
    init_compiler(&compiler, options);

    while (options.input_files[compiler.file] != NULL && compiler.file < options.input_file_count) {
        char *contents = open_file(options.input_files[compiler.file]);
        if (!contents) {
            fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file: %s\n", strerror(errno));
            exit(1);
        }
        compile_module(&compiler, contents, options.input_files[compiler.file]);
        compiler.file++;
    }

    if (!compiler.had_error && !compiler.options.emit_asm && !compiler.options.emit_obj && !compiler.options.debug && !compiler.options.print_ir)
        link_files(compiler.options);

    if (!compiler.options.emit_asm && !compiler.options.debug && !compiler.options.print_ir)
        clean_files(&compiler);
    free_arena(&compiler.arena);
}

