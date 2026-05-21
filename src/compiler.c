#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "gen.h"
#include "compiler.h"
#include "lexer.h"
#include "utils.h"

#define COMPILER_EPRINTF(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->prev.line, compiler->lexer->prev.pos, level, __VA_ARGS__); 
#define COMPILER_EPRINTF_AT_CUR(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->cur.line, compiler->lexer->cur.pos, level, __VA_ARGS__); 

void init_compiler(Compiler *compiler, Lexer *lexer) {
    *compiler = (Compiler){0};
    compiler->lexer = lexer;
    init_arena(&compiler->arena, 1024 * 1024);
}

void print_op(Op *op) {
    switch (op->opcode) {
    case OP_PUSH:
        printf("PUSH %llu\n", op->operand);
        break;
    case OP_JMPF:
        printf("JMPF %llu\n", op->operand);
        break;
    case OP_JMP:
        printf("JMP %llu\n", op->operand);
        break;
    case OP_LABEL:
        printf("LABEL %llu\n", op->operand);
        break;
    case OP_FUNC: {
        Hash_Entry *entry = (Hash_Entry *)op->operand;
        printf("FUNC %.*s\n", entry->key_len, entry->key);
        break;
    }
    case OP_CALL: {
        Hash_Entry *entry = (Hash_Entry *)op->operand;
        printf("CALL %.*s\n", entry->key_len, entry->key);
        break;
    }
    case OP_ADD:   printf("ADD\n");   break;
    case OP_SUB:   printf("SUB\n");   break;
    case OP_MUL:   printf("MUL\n");   break;
    case OP_DIV:   printf("DIV\n");   break;
    case OP_MOD:   printf("MOD\n");   break;
    case OP_AND:   printf("AND\n");   break;
    case OP_OR:    printf("OR\n");    break;
    case OP_XOR:   printf("XOR\n");   break;
    case OP_SHL:   printf("SHL\n");   break;
    case OP_SHR:   printf("SHR\n");   break;
    case OP_ROL:   printf("ROL\n");   break;
    case OP_ROR:   printf("ROR\n");   break;
    case OP_NOT:   printf("NOT\n");   break;
    case OP_DUP:   printf("DUP\n");   break;
    case OP_OVER:  printf("OVER\n");  break;
    case OP_DUP2:  printf("DUP2\n");  break;
    case OP_DROP:  printf("DROP\n");  break;
    case OP_SWAP:  printf("SWAP\n");  break;
    case OP_OVER2: printf("OVER2\n"); break;
    case OP_SWAP2: printf("SWAP2\n"); break;
    case OP_NEG:   printf("NEG\n");   break;
    case OP_ABS:   printf("ABS\n");   break;
    case OP_EQ:    printf("EQ\n");    break;
    case OP_LT:    printf("LT\n");    break;
    case OP_LTEQ:  printf("LTEQ\n");  break;
    case OP_GT:    printf("GT\n");    break;
    case OP_GTEQ:  printf("GTEQ\n");  break;
    case OP_LNOT:  printf("LNOT\n");  break;
    case OP_RET:   printf("RET\n");   break;
    }
}

void print_ops(Ops *ops) {
    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        printf("%s:%d:%d: ", op->file_path, op->line, op->pos);
        print_op(op);
    }
}

inline void make_op(Compiler *compiler, Opcode opcode, int64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .pos = compiler->lexer->prev.pos,
        .line = compiler->lexer->prev.line,
    };
    DA_APPEND(&compiler->ops, op);
}

inline void make_op_at_cur(Compiler *compiler, Opcode opcode, int64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .pos = compiler->lexer->cur.pos,
        .line = compiler->lexer->cur.line,
    };
    DA_APPEND(&compiler->ops, op);
}
static inline void expect(Compiler *compiler, Token_Type type) {
    lexer_next(compiler->lexer);
    if (compiler->lexer->prev.type != type) {
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", tok_spelling(type), tok_spelling(compiler->lexer->prev.type));
    }
}

void compile_stmt(Compiler *compiler);

void compile_if_stmt(Compiler *compiler) {
    int if_start = compiler->ops.count;
    make_op(compiler, OP_JMPF, 0);

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_ELSE && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[if_start].operand = compiler->label_count;

    if (cur->type == TOK_ELSE) {
        lexer_next(compiler->lexer);

        int else_start = compiler->ops.count;
        make_op(compiler, OP_JMP, 0);
        make_op(compiler, OP_LABEL, compiler->label_count++);

        while (cur->type != TOK_SEMICOLON && cur->type != TOK_END && cur->type != TOK_EOF)
            compile_stmt(compiler);
        compiler->ops.items[else_start].operand = compiler->label_count;
    }

    if (cur->type == TOK_EOF) {
        compiler->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of if statement, got end of file. Did you forget a semicolon somewhere?\n");
        return;
    }

    lexer_next(compiler->lexer);
    make_op(compiler, OP_LABEL, compiler->label_count++);
}

void compile_while_stmt(Compiler *compiler) {
    compiler->is_in_loop = 1;

    int loop_label = compiler->label_count;
    make_op(compiler, OP_LABEL, compiler->label_count++);

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_LOOP && cur->type != TOK_LBRACE && cur->type != TOK_EOF)
        compile_stmt(compiler);

    int loop_start = compiler->ops.count;
    make_op_at_cur(compiler, OP_JMPF, 0);

    Token_Type end_type;
    if (cur->type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

    if (cur->type == TOK_EOF) {
        compiler->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected start of loop, got end of file\n");
        return;
    }

    lexer_next(compiler->lexer);
    while (cur->type != TOK_RBRACE && cur->type != TOK_END && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[loop_start].operand = compiler->label_count;

    if (compiler->lexer->cur.type == TOK_EOF) {
        compiler->had_error = 1;
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
    make_op(compiler, OP_LABEL, compiler->label_count++);

    compiler->is_in_loop = 0;
}

void compile_loop_stmt(Compiler *compiler) {
    compiler->is_in_loop = 1;

    Token_Type end_type;
    if (compiler->lexer->prev.type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

    int loop_label = compiler->label_count;
    make_op(compiler, OP_LABEL, compiler->label_count++);
    int loop_start = compiler->ops.count;

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_RBRACE && cur->type != TOK_END)
        compile_stmt(compiler);

    if (compiler->lexer->cur.type == TOK_EOF) {
        compiler->had_error = 1;
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
    make_op(compiler, OP_LABEL, compiler->label_count++);

    compiler->is_in_loop = 0;
}

void compile_stmt(Compiler *compiler) {
    lexer_next(compiler->lexer);

    Token *tok = &compiler->lexer->prev;
#ifdef DEBUG
    print_token(*tok);
#endif

    switch (tok->type) {
    case TOK_INT_LIT: make_op(compiler, OP_PUSH, tok->as.integer); break;
    case TOK_ADD:     make_op(compiler, OP_ADD, 0);                break;
    case TOK_SUB:     make_op(compiler, OP_SUB, 0);                break;
    case TOK_MUL:     make_op(compiler, OP_MUL, 0);                break;
    case TOK_DIV:     make_op(compiler, OP_DIV, 0);                break;
    case TOK_MOD:     make_op(compiler, OP_MOD, 0);                break;
    case TOK_AND:     make_op(compiler, OP_AND, 0);                break;
    case TOK_OR:      make_op(compiler, OP_OR, 0);                 break;
    case TOK_XOR:     make_op(compiler, OP_XOR, 0);                break;
    case TOK_SHL:     make_op(compiler, OP_SHL, 0);                break;
    case TOK_SHR:     make_op(compiler, OP_SHR, 0);                break;
    case TOK_ROL:     make_op(compiler, OP_ROL, 0);                break;
    case TOK_ROR:     make_op(compiler, OP_ROR, 0);                break;
    case TOK_NOT:     make_op(compiler, OP_NOT, 0);                break;
    case TOK_DUP:     make_op(compiler, OP_DUP, 0);                break;
    case TOK_OVER:    make_op(compiler, OP_OVER, 0);               break;
    case TOK_DUP2:    make_op(compiler, OP_DUP2, 0);               break;
    case TOK_DROP:    make_op(compiler, OP_DROP, 0);               break;
    case TOK_SWAP:    make_op(compiler, OP_SWAP, 0);               break;
    case TOK_OVER2:   make_op(compiler, OP_OVER2, 0);              break;
    case TOK_SWAP2:   make_op(compiler, OP_SWAP2, 0);              break;
    case TOK_NEG:     make_op(compiler, OP_NEG, 0);                break;
    case TOK_ABS:     make_op(compiler, OP_ABS, 0);                break;
    case TOK_EQ:      make_op(compiler, OP_EQ, 0);                 break;
    case TOK_LT:      make_op(compiler, OP_LT, 0);                 break;
    case TOK_LTEQ:    make_op(compiler, OP_LTEQ, 0);               break;
    case TOK_GT:      make_op(compiler, OP_GT, 0);                 break;
    case TOK_GTEQ:    make_op(compiler, OP_GTEQ, 0);               break;
    case TOK_LNOT:    make_op(compiler, OP_LNOT, 0);               break;
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
            compiler->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Brk can only be in loops\n");
        }
        compiler->brks.positions[compiler->brks.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_CONTINUE:
        if (!compiler->is_in_loop) {
            compiler->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Continue can only be in loops\n");
        }
        compiler->conts.positions[compiler->conts.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_RET:
        compiler->rets.positions[compiler->rets.count++] = compiler->ops.count;
        make_op(compiler, OP_JMP, -1);
        break;
    case TOK_WORD: {
        Hash_Entry *entry = hashmap_get(&compiler->functions, tok->start, tok->len);
        if (!entry->key) {
            compiler->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Unknown function \'%.*s\'\n", tok->len, tok->start);
            break;
        }
        make_op(compiler, OP_CALL, (int64_t)entry);
        break;
    }
    case TOK_ERROR:
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", tok->len, tok->start);
        break;
    case TOK_ELSE:
    case TOK_SEMICOLON:
    case TOK_END:
    case TOK_RBRACE:
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Lone %s\n", tok_spelling(tok->type));
        break;
    case TOK_FUNC:
    case TOK_EOF:
        eprintf(__FILE__, __LINE__, 0, LEVEL_ERR, "Invalid token %s reached in compile_stmt\n", tok_spelling(tok->type));
        exit(1);
    default:
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Unimplemented operation starting with token %s\n", tok_spelling(tok->type));
        break;
    }
}

void compile_function(Compiler *compiler) {
    lexer_next(compiler->lexer);
    expect(compiler, TOK_WORD);

    Function *func = arena_calloc(&compiler->arena, sizeof(Function));

    Hash_Entry *entry = hashmap_add(&compiler->functions, compiler->lexer->prev.start, compiler->lexer->prev.len, func);
    if (!entry) {
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Redefinition of a function\n");
    }
    make_op(compiler, OP_FUNC, (int64_t)entry);

    expect(compiler, TOK_LPAREN);
    while (compiler->lexer->cur.type != TOK_ARROW && compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        expect(compiler, TOK_WORD);
        func->arity++;
    }

    expect(compiler, TOK_ARROW);
    if (compiler->lexer->prev.type == TOK_EOF) return;

    while (compiler->lexer->cur.type != TOK_RPAREN && compiler->lexer->cur.type != TOK_EOF) {
        expect(compiler, TOK_WORD);
        func->ret_arity++;
    }

    expect(compiler, TOK_RPAREN);
    if (compiler->lexer->prev.type == TOK_EOF) return;

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
        compiler->had_error = 1;
        COMPILER_EPRINTF_AT_CUR(LEVEL_ERR, "Expected end of function, got end of file\n");
    }
}

void compile_functions(Compiler *compiler) {
    while (compiler->lexer->cur.type == TOK_FUNC)
        compile_function(compiler);

    if (compiler->functions.entries != NULL && hashmap_get(&compiler->functions, "main", 4)->key) {
        if (compiler->lexer->cur.type != TOK_EOF) {
            compiler->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Extra tokens at end of file\n");
        }
        return;
    }

    Function *main = arena_calloc(&compiler->arena, sizeof(Function));
    main->arity = 0;
    main->ret_arity = 1;

    Hash_Entry *entry = hashmap_add(&compiler->functions, "main", 4, main);

    make_op_at_cur(compiler, OP_FUNC, (int64_t)entry);
    while (compiler->lexer->cur.type != TOK_FUNC && compiler->lexer->cur.type != TOK_EOF)
        compile_stmt(compiler);

    if (compiler->lexer->cur.type == TOK_FUNC) {
        compiler->had_error++;
        COMPILER_EPRINTF(LEVEL_ERR, "No extra functions can be defined after an implicit main yet\n");
    }

    for (int i = compiler->rets.count-1; i >= 0; i--) {
        compiler->ops.items[compiler->rets.positions[i]].operand = compiler->label_count;
        compiler->rets.count--;
    }

    make_op_at_cur(compiler, OP_LABEL, compiler->label_count++);
    make_op_at_cur(compiler, OP_RET, 0);
}

void compile(const char *src, Compiler_Options options) {
    Lexer lexer;
    init_lexer(&lexer, src, options.file_path);

    Compiler compiler;
    init_compiler(&compiler, &lexer);

    lexer_next(&lexer);
    if (lexer.cur.type == TOK_EOF)
        eprintf(lexer.file_path, lexer.cur.line, lexer.cur.pos, LEVEL_WARN, "Empty file\n");

    compile_functions(&compiler);

    if (!compiler.had_error) {
#ifdef DEBUG
        print_ops(&compiler.ops);
#endif
        generate_x86_64_linux(&compiler.ops, options.output_file);
    }

    free(compiler.ops.items);
    free(compiler.functions.entries);
    free_arena(&compiler.arena);
}

