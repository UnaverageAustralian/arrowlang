#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "analyser.h"
#include "gen.h"
#include "compiler.h"
#include "lexer.h"
#include "utils.h"

#define COMPILER_EPRINTF(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->prev.line, compiler->lexer->prev.pos, level, __VA_ARGS__); 
#define COMPILER_EPRINTF_AT_CUR(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->cur.line, compiler->lexer->cur.pos, level, __VA_ARGS__);

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
    init_arena(&compiler->arena, 1024 * 1024);
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
    case OP_NOP:     return "NOP";
    case OP_PUSH:    return "PUSH";
    case OP_JMPF:    return "JMPF";
    case OP_JMP:     return "JMP";
    case OP_LABEL:   return "LABEL";
    case OP_FUNC:    return "FUNC";
    case OP_CALL:    return "CALL";
    case OP_STR:     return "STR";
    case OP_ADD:     return "ADD";
    case OP_SUB:     return "SUB";
    case OP_MUL:     return "MUL";
    case OP_DIV:     return "DIV";
    case OP_MOD:     return "MOD";
    case OP_AND:     return "AND";
    case OP_OR:      return "OR";
    case OP_XOR:     return "XOR";
    case OP_SHL:     return "SHL";
    case OP_SHR:     return "SHR";
    case OP_ROL:     return "ROL";
    case OP_ROR:     return "ROR";
    case OP_NOT:     return "NOT";
    case OP_DUP:     return "DUP";
    case OP_OVER:    return "OVER";
    case OP_DUP2:    return "DUP2";
    case OP_DROP:    return "DROP";
    case OP_SWAP:    return "SWAP";
    case OP_OVER2:   return "OVER";
    case OP_SWAP2:   return "SWAP";
    case OP_NEG:     return "NEG";
    case OP_EQ:      return "EQ";
    case OP_LT:      return "LT";
    case OP_LTEQ:    return "LTEQ";
    case OP_GT:      return "GT";
    case OP_GTEQ:    return "GTEQ";
    case OP_NEQ:     return "NEQ";
    case OP_LNOT:    return "LNOT";
    case OP_ROT:     return "ROT";
    case OP_ROTN:    return "ROTN";
    case OP_RET:     return "RET";
    case OP_CONVERT: return "CONVERT";
    case OP_CCALL:   return "CCALL";
    case OP_START:   return "START";
    case OP_END:     return "END";
    case OP_IF:      return "IF";
    case OP_ELSE:    return "ELSE";
    default:         return "UNKNOWN";
    }
}

void print_op(Op *op) {
    printf("%s", opcode_spelling(op->opcode));

    switch (op->opcode) {
    case OP_JMPF:
    case OP_JMP:
    case OP_LABEL:
        printf(" %llu", op->operand);
        break;
    case OP_PUSH:
        printf(" %s ", type_spelling(op->types[0]));

        if (op->types[0] == TYPE_REAL)
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
        printf(" %s => %s", type_spelling(op->types[0]), type_spelling(op->types[1]));
        break;
    default:
        if (op->types[1] != TYPE_VOID)
            printf(" %s", type_spelling(op->types[1]));
        if (op->types[0] != TYPE_VOID)
            printf(" %s", type_spelling(op->types[0]));
        break;
    }
    printf("\n");
}

void print_ops(Ops *ops) {
    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        printf("%s:%d:%d: ", op->file_path, op->line, op->pos);
        print_op(op);
    }
}

inline void make_op(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .pos = compiler->lexer->prev.pos,
        .line = compiler->lexer->prev.line,
    };
    DA_APPEND(&compiler->ops, op);
}

inline void make_op_at_cur(Compilation_Unit *compiler, Opcode opcode, uint64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .pos = compiler->lexer->cur.pos,
        .line = compiler->lexer->cur.line,
    };
    DA_APPEND(&compiler->ops, op);
}

static inline void expect(Compilation_Unit *compiler, Token_Type type) {
    lexer_next(compiler->lexer);
    if (compiler->lexer->prev.type != type) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", tok_spelling(type), tok_spelling(compiler->lexer->prev.type));
    }
}

void compile_stmt(Compilation_Unit *compiler);

void compile_if_stmt(Compilation_Unit *compiler) {
    int if_start = compiler->ops.count;
    make_op(compiler, OP_JMPF, 0);
    make_op(compiler, OP_IF, 0);

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_ELSE && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[if_start].operand = compiler->label_count;

    if (cur->type == TOK_ELSE) {
        lexer_next(compiler->lexer);

        int else_start = compiler->ops.count;
        make_op(compiler, OP_JMP, 0);
        make_op_at_cur(compiler, OP_ELSE, compiler->label_count++);

        while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_EOF)
            compile_stmt(compiler);
        compiler->ops.items[else_start].operand = compiler->label_count;
    }

    if (cur->type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of if statement, got end of file. Did you forget a semicolon somewhere?\n");
        return;
    }

    lexer_next(compiler->lexer);
    make_op(compiler, OP_END, compiler->label_count++);
}

void compile_while_stmt(Compilation_Unit *compiler) {
    compiler->is_in_loop = 1;

    int loop_label = compiler->label_count;
    make_op(compiler, OP_START, compiler->label_count++);

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_LOOP && cur->type != TOK_LBRACE && cur->type != TOK_EOF)
        compile_stmt(compiler);

    int loop_start = compiler->ops.count;
    make_op_at_cur(compiler, OP_JMPF, 0);
    make_op_at_cur(compiler, OP_END, compiler->label_count++);

    Token_Type end_type;
    if (cur->type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

    if (cur->type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected start of loop, got end of file\n");
        return;
    }

    lexer_next(compiler->lexer);
    make_op(compiler, OP_START, UINT64_MAX);

    while (cur->type != TOK_RBRACE && cur->type != TOK_END && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[loop_start].operand = compiler->label_count;

    if (compiler->lexer->cur.type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of loop, got end of file. Did you forget an end or right brace?\n");
        return;
    }
    expect(compiler, end_type);

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
    while (cur->type != TOK_RBRACE && cur->type != TOK_END)
        compile_stmt(compiler);

    if (compiler->lexer->cur.type == TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of loop, got end of file. Did you forget an end or right brace?\n");
        return;
    }
    expect(compiler, end_type);

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

Type get_type(Compilation_Unit *compiler) {
    switch (compiler->lexer->prev.type) {
    case TOK_I8:   return TYPE_I8;
    case TOK_CHAR: return TYPE_CHAR;
    case TOK_U8:   return TYPE_U8;
    case TOK_I16:  return TYPE_I16;
    case TOK_U16:  return TYPE_U16;
    case TOK_I32:  return TYPE_I32;
    case TOK_U32:  return TYPE_U32;
    case TOK_I64:  return TYPE_I64;
    case TOK_U64:  return TYPE_U64;
    case TOK_F32:  return TYPE_F32;
    case TOK_F64:  return TYPE_F64;
    case TOK_STR:  return TYPE_STR;
    default:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected type, got %s\n", tok_spelling(compiler->lexer->cur.type));
        return TYPE_VOID;
    }
}

void compile_stmt(Compilation_Unit *compiler) {
    lexer_next(compiler->lexer);

    Token *tok = &compiler->lexer->prev;
#ifdef DEBUG
    print_token(*tok);
#endif

    switch (tok->type) {
    case TOK_INT_LIT:
        make_op(compiler, OP_PUSH, tok->as.integer);
        compiler->ops.items[compiler->ops.count-1].types[0] = TYPE_INTEGER;
        break;
    case TOK_FLOAT_LIT:
        make_op(compiler, OP_PUSH, tok->as.integer);
        compiler->ops.items[compiler->ops.count-1].types[0] = TYPE_REAL;
        break;
    case TOK_CHAR_LIT:
        make_op(compiler, OP_PUSH, tok->as.integer);
        compiler->ops.items[compiler->ops.count-1].types[0] = TYPE_CHAR;
        break;
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
        const char *module_name = tok->start;
        size_t module_name_len = tok->len;

        Hash_Entry *entry = hashmap_get(&compiler->symbols, tok->start, tok->len);
        if (!entry || !entry->key) {
            Unresolved_Symbol sym = {
                .name = { .len = tok->len, .str = tok->start },
                .pos = compiler->ops.count,
            };
            DA_APPEND(&compiler->unresolved, sym);
            make_op(compiler, OP_CALL, 0);
            break;
        }
        if (((Symbol *)entry->val)->type == STYPE_MODULE) {
            expect(compiler, TOK_SCOPE);
            expect(compiler, TOK_WORD);

            entry = hashmap_get(&((Symbol *)entry->val)->as.module.symbols, tok->start, tok->len);
            if (!entry || !entry->key) {
                compiler->global->had_error = 1;
                COMPILER_EPRINTF(LEVEL_ERR, "Unknown function %.*s in module %.*s\n", tok->len, tok->start, module_name_len, module_name);
                return;
            }
        }

        if (((Symbol *)entry->val)->as.func.is_c_func)
            make_op(compiler, OP_CCALL, (int64_t)entry);
        else
            make_op(compiler, OP_CALL, (int64_t)entry);
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
    case TOK_STR:
        make_op(compiler, OP_CONVERT, 0);
        compiler->ops.items[compiler->ops.count-1].types[1] = get_type(compiler);
        break;
    case TOK_ERROR:
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", tok->len, tok->start);
        return;
    case TOK_ELSE:
    case TOK_SEMICOLON:
    case TOK_END:
    case TOK_RBRACE:
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
        eprintf(__FILE__, __LINE__, 0, LEVEL_ERR, "Invalid token %s reached in compile_stmt\n", tok_spelling(tok->type));
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

    Symbol *sym = arena_calloc(&compiler->global->arena, sizeof(Symbol));
    sym->type = STYPE_FUNC;
    sym->as.func.module_name = compiler->module.name;

    Hash_Entry *entry = hashmap_add(&compiler->symbols, compiler->lexer->prev.start, compiler->lexer->prev.len, sym);
    hashmap_add(&compiler->module.symbols, compiler->lexer->prev.start, compiler->lexer->prev.len, sym);
    if (!entry) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Redefinition of a function\n");
    }

    expect(compiler, TOK_LPAREN);
    while (compiler->lexer->cur.type != TOK_ARROW && compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(&sym->as.func.param_types, get_type(compiler));
    }

    expect(compiler, TOK_ARROW);
    if (compiler->lexer->prev.type == TOK_EOF) return entry;

    while (compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        lexer_next(compiler->lexer);
        DA_APPEND(&sym->as.func.return_types, get_type(compiler));
    }

    expect(compiler, TOK_RPAREN);
    if (compiler->lexer->prev.type == TOK_EOF) return entry;

    return entry;
}

void compile_function(Compilation_Unit *compiler) {
    Hash_Entry *entry = compile_function_signature(compiler);
    make_op(compiler, OP_FUNC, (int64_t)entry);

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
    ((Symbol *)entry->val)->as.func.is_c_func = is_c_func;

    if (!compiler->module.has_ext_funcs) {
        compiler->module.has_ext_funcs = 1;
        char *extern_path = arena_calloc(&compiler->global->arena, compiler->module.path.len + compiler->module.name.len + 7);
        snprintf(extern_path, compiler->module.path.len + compiler->module.name.len + 7, "%.*s%.*s_ext.o", compiler->module.path.len, compiler->module.path.str, compiler->module.name.len, compiler->module.name.str);
        if (access(extern_path, F_OK) == 0)
            DA_APPEND(&compiler->global->options.link_cmd, extern_path);
    }
}

void compile_functions(Compilation_Unit *compiler) {
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
        default:;
        }
        break;
    }

    if (compiler->symbols.entries != NULL && hashmap_get(&compiler->symbols, "main", 4)->key) {
        if (compiler->lexer->cur.type != TOK_EOF) {
            compiler->global->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Extra tokens at end of file\n");
        }
        return;
    }

    if (compiler->lexer->cur.type == TOK_EOF) return;

    Symbol *main = arena_calloc(&compiler->global->arena, sizeof(Symbol));
    main->type = STYPE_FUNC;
    DA_APPEND(&main->as.func.return_types, TYPE_U8);
    main->as.func.module_name = (String_View){0};

    Hash_Entry *entry = hashmap_add(&compiler->symbols, "main", 4, main);
    hashmap_add(&compiler->module.symbols, "main", 4, main);

    make_op_at_cur(compiler, OP_FUNC, (int64_t)entry);
    while (compiler->lexer->cur.type != TOK_FUNC && compiler->lexer->cur.type != TOK_EOF)
        compile_stmt(compiler);

    for (int i = compiler->rets.count-1; i >= 0; i--) {
        compiler->ops.items[compiler->rets.positions[i]].operand = compiler->label_count;
        compiler->rets.count--;
    }

    make_op_at_cur(compiler, OP_LABEL, compiler->label_count++);
    make_op_at_cur(compiler, OP_RET, 0);

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
        default:;
        }
        break;
    }

    if (compiler->lexer->cur.type != TOK_EOF) {
        compiler->global->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Extra tokens at end of file\n");
    }
}

void resolve_symbols(Compilation_Unit *compiler) {
    for (size_t i = 0; i < compiler->unresolved.count; i++) {
        Unresolved_Symbol sym = compiler->unresolved.items[i];
        Op *op = &compiler->ops.items[sym.pos];

        Hash_Entry *entry = hashmap_get(&compiler->symbols, sym.name.str, sym.name.len);
        if (!entry || !entry->key || ((Symbol *)entry->val)->type != STYPE_FUNC) {
            compiler->global->had_error = 1;
            eprintf(op->file_path, op->line, op->pos, LEVEL_ERR, "Unknown function %.*s\n", sym.name.len, sym.name.str);
            continue;
        }

        op->operand = (int64_t)entry;
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

    lexer_next(&lexer);
    if (lexer.cur.type == TOK_EOF)
        eprintf(lexer.file_path, lexer.cur.line, lexer.cur.pos, LEVEL_WARN, "Empty file\n");

    while (lexer.cur.type == TOK_IMPORT) {
        lexer_next(&lexer);

        lexer_next(&lexer);
        if (lexer.prev.type == TOK_WORD) {
            char *path = arena_calloc(&global->arena, lexer.prev.len + 12);
            snprintf(path, lexer.prev.len + 12, "./std/%.*s.alng", lexer.prev.len, lexer.prev.start);

            char *contents = open_file(path);
            if (!contents) {
                fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file %s for module %.*s: %s\n", path, lexer.prev.len, lexer.prev.start, strerror(errno));
                exit(1);
            }

            Symbol *sym = compile_module(global, contents, path);
            hashmap_add(&unit.symbols, sym->as.module.name.str, sym->as.module.name.len, sym);
        }
        else if (lexer.prev.type == TOK_STR_LIT) {
            Hash_Entry *entry = hashmap_get(&global->modules, lexer.prev.start, lexer.prev.len);
            if (!entry || !entry->key) {
                eprintf(unit.lexer->file_path, unit.lexer->prev.line, unit.lexer->prev.pos, LEVEL_ERR, "Unresolved module %.*s\n", lexer.prev.len, lexer.prev.start);
                continue;
            }

            Symbol *module = (Symbol *)entry->val;
            hashmap_add(&unit.symbols, entry->key, entry->key_len, module);
        }
        else {
            eprintf(unit.lexer->file_path, unit.lexer->prev.line, unit.lexer->prev.pos, LEVEL_ERR, "Expected word or string, got %s\n", tok_spelling(lexer.prev.type));
        }
    }

    compile_functions(&unit);
    resolve_symbols(&unit);

    if (!global->had_error)
        global->had_error = type_check(&unit.ops);

#ifdef PRINT_IR
    print_ops(&unit.ops);
#else
    if (!global->had_error) {
        char *output_file = arena_calloc(&global->arena, unit.module.name.len + 1);
        strncpy(output_file, unit.module.name.str, unit.module.name.len);

        Hash_Entry *main = hashmap_get(&unit.symbols, "main", 4);
        generate_x86_64_linux(&unit.ops, output_file, main != NULL && main->key != NULL);
    }
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
    cmd_exec(&cmd);
}

void compile(Compiler_Options options) {
    Compiler compiler;
    init_compiler(&compiler, options);

    for (int i = 0; options.input_files[i] != NULL; i++) {
        char *contents = open_file(options.input_files[i]);
        if (!contents) {
            fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file: %s\n", strerror(errno));
            exit(1);
        }
        compile_module(&compiler, contents, options.input_files[i]);
    }

#ifndef PRINT_IR
    if (!compiler.had_error)
        link_files(compiler.options);
#endif

    free_arena(&compiler.arena);
}

