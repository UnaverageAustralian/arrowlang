#include <assert.h>
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

inline String_View strip_file_path(const char *path) {
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

void init_compiler(Compiler *compiler, Compiler_Options options) {
    *compiler = (Compiler){0};
    compiler->options = options;
    init_arena(&compiler->arena, 2 * 1024 * 1024);
}

void init_compilation_unit(Compilation_Unit *unit, Lexer *lexer, Compiler *global) {
    *unit = (Compilation_Unit){0};
    unit->lexer = lexer;
    unit->global = global;

    unit->module.name = strip_file_path(lexer->file_path);
    unit->module.path = (String_View){ .len = unit->module.name.str - lexer->file_path, .str = lexer->file_path, };
    unit->module.symbols = (Hashmap){0};
}

char *opcode_spelling(Opcode opcode) {
    switch (opcode) {
    case OP_NOP:         return "NOP";
    case OP_PUSH:        return "PUSH";
    case OP_JMPF:        return "JMPF";
    case OP_JMP:         return "JMP";
    case OP_LABEL:       return "LABEL";
    case OP_FUNC:        return "FUNC";
    case OP_CALL:        return "CALL";
    case OP_STR:         return "STR";
    case OP_ADD:         return "ADD";
    case OP_SUB:         return "SUB";
    case OP_MUL:         return "MUL";
    case OP_DIV:         return "DIV";
    case OP_MOD:         return "MOD";
    case OP_AND:         return "AND";
    case OP_OR:          return "OR";
    case OP_XOR:         return "XOR";
    case OP_SHL:         return "SHL";
    case OP_SHR:         return "SHR";
    case OP_ROL:         return "ROL";
    case OP_ROR:         return "ROR";
    case OP_NOT:         return "NOT";
    case OP_DUP:         return "DUP";
    case OP_OVER:        return "OVER";
    case OP_DUP2:        return "DUP2";
    case OP_DROP:        return "DROP";
    case OP_SWAP:        return "SWAP";
    case OP_OVER2:       return "OVER";
    case OP_SWAP2:       return "SWAP2";
    case OP_NEG:         return "NEG";
    case OP_EQ:          return "EQ";
    case OP_LT:          return "LT";
    case OP_LTEQ:        return "LTEQ";
    case OP_GT:          return "GT";
    case OP_GTEQ:        return "GTEQ";
    case OP_NEQ:         return "NEQ";
    case OP_LNOT:        return "LNOT";
    case OP_ROT:         return "ROT";
    case OP_ROTN:        return "ROTN";
    case OP_RET:         return "RET";
    case OP_CONVERT:     return "CONVERT";
    case OP_CCALL:       return "CCALL";
    case OP_START:       return "START";
    case OP_END:         return "END";
    case OP_IF:          return "IF";
    case OP_ELSE:        return "ELSE";
    case OP_ELSEIF:      return "ELSEIF";
    case OP_INIT:        return "INIT";
    case OP_ACCESS:      return "ACCESS";
    case OP_STORE:       return "STORE";
    case OP_ACCESS_DROP: return "ACCESS_DROP";
    default:             return "UNKNOWN";
    }
}

char *err_opcode_spelling(Opcode opcode) {
    switch (opcode) {
    case OP_JMPF:        return "conditional branch";
    case OP_CCALL:
    case OP_CALL:        return "function call";
    case OP_ADD:         return "addition";
    case OP_SUB:         return "subtraction";
    case OP_MUL:         return "multiplication";
    case OP_DIV:         return "division";
    case OP_MOD:         return "mod";
    case OP_AND:         return "and";
    case OP_OR:          return "or";
    case OP_XOR:         return "xor";
    case OP_SHL:         return "shl";
    case OP_SHR:         return "shr";
    case OP_ROL:         return "rol";
    case OP_ROR:         return "ror";
    case OP_NOT:         return "not";
    case OP_DUP:         return "dup";
    case OP_OVER:        return "over";
    case OP_DUP2:        return "dup2";
    case OP_DROP:        return "drop";
    case OP_SWAP:        return "swap";
    case OP_OVER2:       return "over";
    case OP_SWAP2:       return "swap2";
    case OP_NEG:         return "neg";
    case OP_EQ:          return "eq";
    case OP_LT:          return "lt";
    case OP_LTEQ:        return "lteq";
    case OP_GT:          return "gt";
    case OP_GTEQ:        return "gteq";
    case OP_NEQ:         return "neq";
    case OP_LNOT:        return "lnot";
    case OP_ROT:         return "rot";
    case OP_ROTN:        return "rotn";
    case OP_RET:         return "returning";
    case OP_CONVERT:     return "type conversion";
    case OP_INIT:        return "struct initialisation";
    case OP_ACCESS_DROP:
    case OP_ACCESS:      return "struct accessing";
    case OP_STORE:       return "struct storing";
    default:             return "unknown operation";
    }
}

void print_op(Op *op) {
    printf("%s", opcode_spelling(op->opcode));

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

        if (op->types[0].as.basic == TYPE_REAL)
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
            printf(" %.*s::%.*s", func.module_name.len, func.module_name.str, entry->key_len, entry->key);
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
        printf(" %.*s", sv->len, sv->str);
        break;
    }
    default:
        if (op->types[1].as.basic != TYPE_VOID)
            printf(" %s", type_spelling(op->types[1]));
        if (op->types[0].as.basic != TYPE_VOID)
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

inline Hash_Entry *add_symbol(Compilation_Unit *compiler, Symbol *sym) {
    Hash_Entry *entry = hashmap_add(&compiler->symbols, compiler->lexer->prev.start, compiler->lexer->prev.len, sym);
    hashmap_add(&compiler->module.symbols, compiler->lexer->prev.start, compiler->lexer->prev.len, sym);
    if (!entry) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Redefinition of a symbol\n");
    }
    return entry;
}

inline Op *make_op(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .loc = compiler->lexer->prev.loc,
    };
    DA_APPEND(&compiler->ops, op);
    return &compiler->ops.items[compiler->ops.count-1];
}

inline Op *make_op_at_cur(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .loc = compiler->lexer->cur.loc,
    };
    DA_APPEND(&compiler->ops, op);
    return &compiler->ops.items[compiler->ops.count-1];
}

inline Unresolved_Symbol *make_unresolved(Compilation_Unit *compiler, Unresolved_Type type) {
    Unresolved_Symbol sym = {
        .name = { .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start },
        .loc = compiler->lexer->prev.loc,
        .type = type,
    };
    DA_APPEND(&compiler->unresolved, sym);
    return &compiler->unresolved.items[compiler->unresolved.count-1];
}

static inline int expect(Compilation_Unit *compiler, Token_Type type) {
    lexer_next(compiler->lexer);
    if (compiler->lexer->prev.type != type) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", err_tok_spelling(type), err_tok_spelling(compiler->lexer->prev.type));
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

        ARENA_DA_APPEND(&compiler->global->arena, &elseifs, compiler->ops.count);
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
    compiler->global->arena.allocated -= elseifs.count * sizeof(int);

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

    Token_Type end_type;
    if (cur->type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

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

    Token_Type end_type;
    if (compiler->lexer->prev.type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

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

void compile_struct_fields(Compilation_Unit *compiler, Struct *structure);

Advanced_Type *compile_anonymous_struct(Compilation_Unit *compiler) {
    Advanced_Type type = {0};
    type.kind = KIND_STRUCT;

    compile_struct_fields(compiler, &type.as.structure);
    DA_APPEND(&compiler->types, type);
    return &compiler->types.items[compiler->types.count-1];
}

void get_type(Compilation_Unit *compiler, Type *type) {
    switch (compiler->lexer->prev.type) {
    case TOK_I8:   *type = BASIC_TYPE(TYPE_I8);   break;
    case TOK_CHAR: *type = BASIC_TYPE(TYPE_CHAR); break;
    case TOK_U8:   *type = BASIC_TYPE(TYPE_U8);   break;
    case TOK_I16:  *type = BASIC_TYPE(TYPE_I16);  break;
    case TOK_U16:  *type = BASIC_TYPE(TYPE_U16);  break;
    case TOK_I32:  *type = BASIC_TYPE(TYPE_I32);  break;
    case TOK_U32:  *type = BASIC_TYPE(TYPE_U32);  break;
    case TOK_I64:  *type = BASIC_TYPE(TYPE_I64);  break;
    case TOK_U64:  *type = BASIC_TYPE(TYPE_U64);  break;
    case TOK_F32:  *type = BASIC_TYPE(TYPE_F32);  break;
    case TOK_F64:  *type = BASIC_TYPE(TYPE_F64);  break;
    case TOK_STR:  *type = BASIC_TYPE(TYPE_STR);  break;
    case TOK_WORD: {
        Hash_Entry *entry = hashmap_get(&compiler->symbols, compiler->lexer->prev.start, compiler->lexer->prev.len);
        if (!entry || !entry->key) {
            Unresolved_Symbol *unresolved = make_unresolved(compiler, UTYPE_TYPE);
            unresolved->as.type = type;
            break;
        }

        Symbol *sym = (Symbol *)entry->val;
        if (sym->type != STYPE_TYPE) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "%.*s is not a structure\n", entry->key_len, entry->key);
            *type = BASIC_TYPE(TYPE_VOID);
            break;
        }

        *type = ADVANCED_TYPE(sym->as.type);
        break;
    }
    case TOK_STRUCT:
        *type = ADVANCED_TYPE(compile_anonymous_struct(compiler));
        break;
    default:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected type, got %s\n", err_tok_spelling(compiler->lexer->prev.type));
        *type = BASIC_TYPE(TYPE_VOID);
        break;
    }
}

void compile_entry(Compilation_Unit *compiler, Hash_Entry *entry) {
    Token *tok = &compiler->lexer->prev;

    if (!entry || !entry->key) {
        Unresolved_Symbol *unresolved = make_unresolved(compiler, UTYPE_OP);
        unresolved->as.op = &compiler->ops.items[compiler->ops.count];
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
        op->types[0] = ADVANCED_TYPE(sym->as.type);
        break;
    }
    case STYPE_MODULE: {
        const char *module_name = tok->start;
        size_t module_name_len = tok->len;

        expect(compiler, TOK_SCOPE);
        if (compiler->lexer->prev.type == TOK_EOF) return;

        if (!expect(compiler, TOK_WORD)) return;

        entry = hashmap_get(&((Symbol *)entry->val)->as.module.symbols, tok->start, tok->len);
        if (!entry || !entry->key) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Unknown symbol %.*s in module %.*s\n", tok->len, tok->start, module_name_len, module_name);
            return;
        }

        compile_entry(compiler, entry);
        break;
    }
    }
}

void compile_stmt(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);

    Token *tok = &compiler->lexer->prev;
    switch (tok->type) {
    case TOK_INT_LIT: {
        Op *op = make_op(compiler, OP_PUSH, tok->as.integer);
        op->types[0] = BASIC_TYPE(TYPE_INTEGER);
        break;
    }
    case TOK_FLOAT_LIT: {
        Op *op = make_op(compiler, OP_PUSH, tok->as.integer);
        op->types[0] = BASIC_TYPE(TYPE_REAL);
        break;
    }
    case TOK_CHAR_LIT: {
        Op *op = make_op(compiler, OP_PUSH, tok->as.integer);
        op->types[0] = BASIC_TYPE(TYPE_CHAR);
        break;
    }
    case TOK_ADD:   make_op(compiler, OP_ADD, 0);   break;
    case TOK_SUB:   make_op(compiler, OP_SUB, 0);   break;
    case TOK_MUL:   make_op(compiler, OP_MUL, 0);   break;
    case TOK_DIV:   make_op(compiler, OP_DIV, 0);   break;
    case TOK_MOD:   make_op(compiler, OP_MOD, 0);   break;
    case TOK_AND:   make_op(compiler, OP_AND, 0);   break;
    case TOK_OR:    make_op(compiler, OP_OR, 0);    break;
    case TOK_XOR:   make_op(compiler, OP_XOR, 0);   break;
    case TOK_SHL:   make_op(compiler, OP_SHL, 0);   break;
    case TOK_SHR:   make_op(compiler, OP_SHR, 0);   break;
    case TOK_ROL:   make_op(compiler, OP_ROL, 0);   break;
    case TOK_ROR:   make_op(compiler, OP_ROR, 0);   break;
    case TOK_NOT:   make_op(compiler, OP_NOT, 0);   break;
    case TOK_DUP:   make_op(compiler, OP_DUP, 0);   break;
    case TOK_OVER:  make_op(compiler, OP_OVER, 0);  break;
    case TOK_DUP2:  make_op(compiler, OP_DUP2, 0);  break;
    case TOK_DROP:  make_op(compiler, OP_DROP, 0);  break;
    case TOK_SWAP:  make_op(compiler, OP_SWAP, 0);  break;
    case TOK_OVER2: make_op(compiler, OP_OVER2, 0); break;
    case TOK_SWAP2: make_op(compiler, OP_SWAP2, 0); break;
    case TOK_NEG:   make_op(compiler, OP_NEG, 0);   break;
    case TOK_EQ:    make_op(compiler, OP_EQ, 0);    break;
    case TOK_LT:    make_op(compiler, OP_LT, 0);    break;
    case TOK_LTEQ:  make_op(compiler, OP_LTEQ, 0);  break;
    case TOK_GT:    make_op(compiler, OP_GT, 0);    break;
    case TOK_GTEQ:  make_op(compiler, OP_GTEQ, 0);  break;
    case TOK_NEQ:   make_op(compiler, OP_NEQ, 0);   break;
    case TOK_LNOT:  make_op(compiler, OP_LNOT, 0);  break;
    case TOK_ROT:   make_op(compiler, OP_ROT, 0);   break;
    case TOK_ROTN:  make_op(compiler, OP_ROTN, 0);  break;
    case TOK_STR_LIT: {
        char *str = arena_calloc(&compiler->global->arena, tok->len + 1);
        strncpy(str, tok->start, tok->len);
        make_op(compiler, OP_STR, (int64_t)str);
        break;
    }
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
    case TOK_BRK:
        if (!compiler->is_in_loop) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Brk can only be in loops\n");
            return;
        }
        compiler->brks.positions[compiler->brks.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_CONTINUE:
        if (!compiler->is_in_loop) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Continue can only be in loops\n");
            return;
        }
        compiler->conts.positions[compiler->conts.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_RET:
        compiler->rets.positions[compiler->rets.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_WORD: {
        Hash_Entry *entry = hashmap_get(&compiler->symbols, tok->start, tok->len);
        compile_entry(compiler, entry);
        break;
    }
    case TOK_STRUCT: {
        Op *op = make_op(compiler, OP_INIT, 0);
        op->types[0].kind = KIND_ADVANCED;
        op->types[0].as.advanced = compile_anonymous_struct(compiler);
        break;
    }
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
    case TOK_STR: {
        Op *op = make_op(compiler, OP_CONVERT, 0);
        get_type(compiler, &op->types[1]);
        break;
    }
    case TOK_HASH: {
        if (!expect(compiler, TOK_WORD)) return;
        String_View *sv = arena_calloc(&compiler->global->arena, sizeof(String_View));
        *sv = (String_View){ .len = tok->len, .str = tok->start };
        make_op(compiler, OP_ACCESS, (uint64_t)sv);
        break;
    }
    case TOK_ARROW_HASH: {
        if (!expect(compiler, TOK_WORD)) return;
        String_View *sv = arena_calloc(&compiler->global->arena, sizeof(String_View));
        *sv = (String_View){ .len = tok->len, .str = tok->start };
        make_op(compiler, OP_STORE, (uint64_t)sv);
        break;
    }
    case TOK_DOT: {
        if (!expect(compiler, TOK_WORD)) return;
        String_View *sv = arena_calloc(&compiler->global->arena, sizeof(String_View));
        *sv = (String_View){ .len = tok->len, .str = tok->start };
        make_op(compiler, OP_ACCESS_DROP, (uint64_t)sv);
        break;
    }
    case TOK_ERROR:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", tok->len, tok->start);
        return;
    case TOK_COLON:
    case TOK_SEMICOLON:
    case TOK_END:
    case TOK_RBRACE:
    case TOK_SCOPE:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Lone %s\n", err_tok_spelling(tok->type));
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
    default:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Unimplemented operation starting with token %s\n", tok_spelling(tok->type));
        return;
    }
}

Hash_Entry *compile_function_signature(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);

    expect(compiler, TOK_WORD);
    if (compiler->lexer->prev.type == TOK_EOF) return NULL;

    Symbol *sym = arena_calloc(&compiler->global->arena, sizeof(Symbol));
    sym->type = STYPE_FUNC;
    sym->as.func.module_name = compiler->module.name;

    Hash_Entry *entry = add_symbol(compiler, sym);

    expect(compiler, TOK_LPAREN);
    if (compiler->lexer->prev.type == TOK_EOF) return entry;

    while (compiler->lexer->cur.type != TOK_ARROW && compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(&sym->as.func.param_types, (Type){0});
        get_type(compiler, &sym->as.func.param_types.items[sym->as.func.param_types.count-1]);
    }

    expect(compiler, TOK_ARROW);
    if (compiler->lexer->prev.type == TOK_EOF) return entry;

    while (compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(&sym->as.func.return_types, (Type){0});
        get_type(compiler, &sym->as.func.return_types.items[sym->as.func.return_types.count-1]);
    }

    expect(compiler, TOK_RPAREN);
    if (compiler->lexer->prev.type == TOK_EOF) return entry;

    return entry;
}

void compile_function(Compilation_Unit *compiler) {
    Hash_Entry *entry = compile_function_signature(compiler);
    if (compiler->lexer->prev.type == TOK_EOF) return;

    make_op(compiler, OP_FUNC, (int64_t)entry);

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

    if (!compiler->module.has_ext_funcs) {
        compiler->module.has_ext_funcs = 1;
        char *extern_path = arena_calloc(&compiler->global->arena, compiler->module.path.len + compiler->module.name.len + 7);
        snprintf(extern_path, compiler->module.path.len + compiler->module.name.len + 7, "%.*s%.*s_ext.o",
                 compiler->module.path.len, compiler->module.path.str, compiler->module.name.len, compiler->module.name.str);
        if (access(extern_path, F_OK) == 0)
            DA_APPEND(&compiler->global->options.link_cmd, extern_path);
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

    if (!expect(compiler, TOK_END)) return;
}

void compile_struct(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);
    expect(compiler, TOK_WORD);
    if (compiler->lexer->prev.type == TOK_EOF) return;

    Symbol *sym = arena_calloc(&compiler->global->arena, sizeof(Symbol));
    sym->type = STYPE_TYPE;

    add_symbol(compiler, sym);
    DA_APPEND(&compiler->types, (Advanced_Type){0});
    sym->as.type = &compiler->types.items[compiler->types.count-1];
    sym->as.type->loc = compiler->lexer->prev.loc;

    sym->as.type->as.structure.name = (String_View){ .len = compiler->lexer->prev.len, .str = compiler->lexer->prev.start };

    compile_struct_fields(compiler, &sym->as.type->as.structure);
}

void compile_decls(Compilation_Unit *compiler) {
    for (; ;) {
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
            compile_struct(compiler);
            continue;
        default: break;
        }
        break;
    }

    Hash_Entry *main = hashmap_get(&compiler->symbols, "main", 4);
    if (main && main->val)
        ((Symbol *)main->val)->as.func.module_name = (String_View){0};

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
    int alignment = 0;

    for (size_t i = 0; i < type->as.structure.fields.count; i++) {
        Field *field = &type->as.structure.fields.items[i];
        if (field->type.kind == KIND_ADVANCED && !resolve_type(compiler, field->type.as.advanced))
            return 0;

        int field_alignment = type_alignment(field->type);
        if (field_alignment > alignment)
            alignment = field_alignment;

        offset = ALIGN(offset, field_alignment);
        field->offset = offset;

        offset += type_size(field->type);
    }

    type->as.structure.alignment = alignment;
    type->as.structure.size = ALIGN(offset, 8);
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

        Hash_Entry *entry = hashmap_get(&compiler->symbols, unresolved.name.str, unresolved.name.len);
        if (!entry || !entry->key) {
            compiler->global->had_error = 1;
            eprintf(compiler->lexer->file_path, unresolved.loc, LEVEL_ERR, "Unknown symbol %.*s\n",
                    unresolved.name.len, unresolved.name.str);
            continue;
        }

        Symbol *sym = (Symbol *)entry->val;

        switch (unresolved.type) {
        case UTYPE_OP: {
            Op *op = unresolved.as.op;
            switch (sym->type) {
            case STYPE_FUNC:
                op->opcode = sym->as.func.is_c_func ? OP_CCALL : OP_CALL;
                op->operand = (int64_t)entry;
                break;
            case STYPE_TYPE:
                op->opcode = OP_INIT;
                op->types[0] = ADVANCED_TYPE(sym->as.type);
                break;
            case STYPE_MODULE:
                compiler->global->had_error = 1;
                COMPILER_EPRINTF(LEVEL_ERR, "Unreachable. Please report this as a bug.");
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

            *unresolved.as.type = ADVANCED_TYPE(sym->as.type);
            break;
        }
        }
    }
}

Symbol *compile_module(Compiler *global, const char *src, const char *file_path) {
    Lexer lexer;
    init_lexer(&lexer, src, file_path);

    Compilation_Unit unit;
    init_compilation_unit(&unit, &lexer, global);

    Hash_Entry *entry = hashmap_get(&global->modules, unit.module.name.str, unit.module.name.len);
    if (entry && entry->key) return (Symbol *)entry->val;

    char *obj_name = arena_calloc(&global->arena, unit.module.name.len + 3);
    snprintf(obj_name, unit.module.name.len + 3, "%.*s.o", unit.module.name.len, unit.module.name.str);

    DA_APPEND(&global->options.link_cmd, obj_name);
    DA_APPEND(&global->cleanup, obj_name);

    lexer_next(&lexer);
    if (lexer.cur.type == TOK_EOF)
        eprintf(lexer.file_path, lexer.cur.loc, LEVEL_WARN, "Empty file\n");

    while (lexer.cur.type == TOK_IMPORT) {
        lexer_next(&lexer);

        lexer_next(&lexer);
        if (lexer.prev.type == TOK_WORD) {
            char *path = arena_calloc(&global->arena, lexer.prev.len + 12);
            snprintf(path, lexer.prev.len + 12, "./std/%.*s.alng", lexer.prev.len, lexer.prev.start);

            char *contents = open_file(path);
            if (!contents) {
                fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file %s for module %.*s: %s\n",
                        path, lexer.prev.len, lexer.prev.start, strerror(errno));
                exit(1);
            }

            Symbol *sym = compile_module(global, contents, path);
            hashmap_add(&unit.symbols, sym->as.module.name.str, sym->as.module.name.len, sym);
        }
        else if (lexer.prev.type == TOK_STR_LIT) {
            Hash_Entry *entry = hashmap_get(&global->modules, lexer.prev.start, lexer.prev.len);
            if (!entry || !entry->key) {
                global->had_error = 1;
                eprintf(unit.lexer->file_path, unit.lexer->prev.loc, LEVEL_ERR, "Unresolved module %.*s\n", lexer.prev.len, lexer.prev.start);
                continue;
            }

            Symbol *module = (Symbol *)entry->val;
            hashmap_add(&unit.symbols, entry->key, entry->key_len, module);
        }
        else {
            eprintf(unit.lexer->file_path, unit.lexer->prev.loc, LEVEL_ERR, "Expected word or string, got %s\n", err_tok_spelling(lexer.prev.type));
        }
    }

    compile_decls(&unit);

    resolve_symbols(&unit);
    resolve_types(&unit);

#ifndef DEBUG
    if (!global->had_error)
        global->had_error = type_check(&unit.ops);

#ifdef PRINT_IR
    print_ops(&unit.ops);
#else
    if (!global->had_error) {
        Hash_Entry *main = hashmap_get(&unit.symbols, "main", 4);
        char *output_asm = generate_x86_64_linux(&unit.ops, obj_name, main != NULL && main->key != NULL);
        DA_APPEND(&global->cleanup, output_asm);

        if (!global->options.emit_asm) {
            Cmd cmd = {0};
            cmd_append_many(&cmd, 4, "as", "-o", obj_name, output_asm);
            cmd_exec(&cmd, global->options.verbose);
        }
    }
#endif

#else
    print_ops(&unit.ops);
#endif

    Symbol *module_sym = arena_calloc(&global->arena, sizeof(Symbol));
    module_sym->type = STYPE_MODULE;
    module_sym->as.module = unit.module;

    hashmap_add(&global->modules, unit.module.name.str, unit.module.name.len, module_sym);

    free(unit.ops.items);
    free(unit.symbols.entries);

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

    for (int i = 0; options.input_files[i] != NULL && i < options.input_file_count; i++) {
        char *contents = open_file(options.input_files[i]);
        if (!contents) {
            fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file: %s\n", strerror(errno));
            exit(1);
        }
        compile_module(&compiler, contents, options.input_files[i]);
    }

#if !defined(PRINT_IR) && !defined(DEBUG)
    if (!compiler.had_error && !compiler.options.emit_asm)
        link_files(compiler.options);
#endif

    if (!compiler.options.emit_asm)
        clean_files(&compiler);
    free_arena(&compiler.arena);
}

