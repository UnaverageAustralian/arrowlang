#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "gen.h"
#include "compiler.h"
#include "lexer.h"
#include "utils.h"

#define COMPILER_EPRINTF(level, ...) eprintf(compiler->lexer->file_path, compiler->lexer->prev.line, compiler->lexer->prev.pos, level, __VA_ARGS__); 

void init_compiler(Compiler *compiler, Lexer *lexer) {
    compiler->lexer = lexer;
    compiler->ops = (Ops){0};
    compiler->label_count = 0;
    compiler->backpatchees_count = 0;
    compiler->had_error = 0;
    compiler->is_in_loop = 0;
}

void print_op(Op *op) {
    switch (op->opcode) {
    case OP_PUSH:
        printf("PUSH %lld\n", op->operand);
        break;
    case OP_JMPF:
        printf("JMPF %lld\n", op->operand);
        break;
    case OP_JMP:
        printf("JMP %lld\n", op->operand);
        break;
    case OP_LABEL:
        printf("LABEL %lld\n", op->operand);
        break;
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
    }
}

void print_ops(Ops *ops) {
    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        printf("%s:%d:%d: ", op->file_path, op->line, op->pos);
        print_op(op);
    }
}

inline Op make_op(Compiler *compiler, Opcode opcode, int64_t operand) {
    Op op = {
        .opcode = opcode,
        .operand = operand,
        .file_path = compiler->lexer->file_path,
        .pos = compiler->lexer->prev.pos,
        .line = compiler->lexer->prev.line,
    };
    return op;
}

void compile_stmt(Compiler *compiler);

void compile_if_stmt(Compiler *compiler) {
    int if_start = compiler->ops.count;
    DA_APPEND(&compiler->ops, make_op(compiler, OP_JMPF, 0));

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_SEMICOLON && cur->type != TOK_ELSE && cur->type != TOK_EOF)
        compile_stmt(compiler);
    compiler->ops.items[if_start].operand = compiler->label_count;

    if (cur->type == TOK_ELSE) {
        lexer_next(compiler->lexer);

        int else_start = compiler->ops.count;
        DA_APPEND(&compiler->ops, make_op(compiler, OP_JMP, 0));
        DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));

        while (cur->type != TOK_SEMICOLON && cur->type != TOK_EOF)
            compile_stmt(compiler);
        compiler->ops.items[else_start].operand = compiler->label_count;
    }

    if (cur->type == TOK_EOF) {
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected end of if statement, got end of file. Did you forget a semicolon somewhere?\n");
        return;
    }

    lexer_next(compiler->lexer);
    DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));
}

void compile_while_stmt(Compiler *compiler) {
    compiler->is_in_loop = 1;

    int loop_label = compiler->label_count;
    DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_LOOP && cur->type != TOK_LBRACE && cur->type != TOK_EOF)
        compile_stmt(compiler);

    int loop_start = compiler->ops.count;
    DA_APPEND(&compiler->ops, make_op(compiler, OP_JMPF, 0));

    Token_Type end_type;
    if (cur->type == TOK_LBRACE)
        end_type = TOK_RBRACE;
    else
        end_type = TOK_END;

    if (cur->type == TOK_EOF) {
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Expected start of loop, got end of file\n");
        return;
    }

    lexer_next(compiler->lexer);
    while (cur->type != TOK_RBRACE && cur->type != TOK_END)
        compile_stmt(compiler);
    compiler->ops.items[loop_start].operand = compiler->label_count;

    if (cur->type != end_type) {
        compiler->had_error = 1;
        lexer_next(compiler->lexer);
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", tok_spelling(end_type), tok_spelling(compiler->lexer->prev.type));
        return;
    }

    DA_APPEND(&compiler->ops, make_op(compiler, OP_JMP, loop_label));

    lexer_next(compiler->lexer);
    for (int i = compiler->backpatchees_count-1; i >= 0; i--) {
        if (compiler->backpatchees[i].pos < loop_start) break;
        compiler->ops.items[compiler->backpatchees[i].pos].operand =
            compiler->backpatchees[i].type == BTYPE_BRK ? compiler->label_count : loop_label;
        compiler->backpatchees_count--;
    }
    DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));

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
    DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));
    int loop_start = compiler->ops.count;

    Token *cur = &compiler->lexer->cur;
    while (cur->type != TOK_RBRACE && cur->type != TOK_END)
        compile_stmt(compiler);

    if (cur->type != end_type) {
        compiler->had_error = 1;
        lexer_next(compiler->lexer);
        COMPILER_EPRINTF(LEVEL_ERR, "Expected %s, got %s\n", tok_spelling(end_type), tok_spelling(compiler->lexer->prev.type));
        return;
    }
    DA_APPEND(&compiler->ops, make_op(compiler, OP_JMP, loop_label));

    lexer_next(compiler->lexer);
    for (int i = compiler->backpatchees_count-1; i >= 0; i--) {
        if (compiler->backpatchees[i].pos < loop_start) break;
        compiler->ops.items[compiler->backpatchees[i].pos].operand =
            compiler->backpatchees[i].type == BTYPE_BRK ? compiler->label_count : loop_label;
        compiler->backpatchees_count--;
    }
    DA_APPEND(&compiler->ops, make_op(compiler, OP_LABEL, compiler->label_count++));

    compiler->is_in_loop = 0;
}

inline int is_invalid_tok_type(Token_Type type) {
    return type == TOK_EOF || type == TOK_ELSE || type == TOK_SEMICOLON
        || type == TOK_END || type == TOK_RBRACE;
}

void compile_stmt(Compiler *compiler) {
    assert(!is_invalid_tok_type(compiler->lexer->cur.type));
    lexer_next(compiler->lexer);

    Token *tok = &compiler->lexer->prev;
    print_token(*tok);

    switch (tok->type) {
    case TOK_INT_LIT: DA_APPEND(&compiler->ops, make_op(compiler, OP_PUSH, tok->as.integer)); break;
    case TOK_ADD:     DA_APPEND(&compiler->ops, make_op(compiler, OP_ADD, 0));                break;
    case TOK_SUB:     DA_APPEND(&compiler->ops, make_op(compiler, OP_SUB, 0));                break;
    case TOK_MUL:     DA_APPEND(&compiler->ops, make_op(compiler, OP_MUL, 0));                break;
    case TOK_DIV:     DA_APPEND(&compiler->ops, make_op(compiler, OP_DIV, 0));                break;
    case TOK_MOD:     DA_APPEND(&compiler->ops, make_op(compiler, OP_MOD, 0));                break;
    case TOK_AND:     DA_APPEND(&compiler->ops, make_op(compiler, OP_AND, 0));                break;
    case TOK_OR:      DA_APPEND(&compiler->ops, make_op(compiler, OP_OR, 0));                 break;
    case TOK_XOR:     DA_APPEND(&compiler->ops, make_op(compiler, OP_XOR, 0));                break;
    case TOK_SHL:     DA_APPEND(&compiler->ops, make_op(compiler, OP_SHL, 0));                break;
    case TOK_SHR:     DA_APPEND(&compiler->ops, make_op(compiler, OP_SHR, 0));                break;
    case TOK_ROL:     DA_APPEND(&compiler->ops, make_op(compiler, OP_ROL, 0));                break;
    case TOK_ROR:     DA_APPEND(&compiler->ops, make_op(compiler, OP_ROR, 0));                break;
    case TOK_NOT:     DA_APPEND(&compiler->ops, make_op(compiler, OP_NOT, 0));                break;
    case TOK_DUP:     DA_APPEND(&compiler->ops, make_op(compiler, OP_DUP, 0));                break;
    case TOK_OVER:    DA_APPEND(&compiler->ops, make_op(compiler, OP_OVER, 0));               break;
    case TOK_DUP2:    DA_APPEND(&compiler->ops, make_op(compiler, OP_DUP2, 0));               break;
    case TOK_DROP:    DA_APPEND(&compiler->ops, make_op(compiler, OP_DROP, 0));               break;
    case TOK_SWAP:    DA_APPEND(&compiler->ops, make_op(compiler, OP_SWAP, 0));               break;
    case TOK_OVER2:   DA_APPEND(&compiler->ops, make_op(compiler, OP_OVER2, 0));              break;
    case TOK_SWAP2:   DA_APPEND(&compiler->ops, make_op(compiler, OP_SWAP2, 0));              break;
    case TOK_NEG:     DA_APPEND(&compiler->ops, make_op(compiler, OP_NEG, 0));                break;
    case TOK_ABS:     DA_APPEND(&compiler->ops, make_op(compiler, OP_ABS, 0));                break;
    case TOK_EQ:      DA_APPEND(&compiler->ops, make_op(compiler, OP_EQ, 0));                 break;
    case TOK_LT:      DA_APPEND(&compiler->ops, make_op(compiler, OP_LT, 0));                 break;
    case TOK_LTEQ:    DA_APPEND(&compiler->ops, make_op(compiler, OP_LTEQ, 0));               break;
    case TOK_GT:      DA_APPEND(&compiler->ops, make_op(compiler, OP_GT, 0));                 break;
    case TOK_GTEQ:    DA_APPEND(&compiler->ops, make_op(compiler, OP_GTEQ, 0));               break;
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
        compiler->backpatchees[compiler->backpatchees_count++] = (Backpatchee){
            .pos = compiler->ops.count,
            .type = BTYPE_BRK,
        };
        DA_APPEND(&compiler->ops, make_op(compiler, OP_JMP, -1));
        break;
    case TOK_CONTINUE:
        if (!compiler->is_in_loop) {
            compiler->had_error = 1;
            COMPILER_EPRINTF(LEVEL_ERR, "Continue can only be in loops\n");
        }
        compiler->backpatchees[compiler->backpatchees_count++] = (Backpatchee){
            .pos = compiler->ops.count,
            .type = BTYPE_CONT,
        };
        DA_APPEND(&compiler->ops, make_op(compiler, OP_JMP, -1));
        break;
    case TOK_ERROR:
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "%.*s\n", tok->len, tok->start);
        break;
    default:
        compiler->had_error = 1;
        COMPILER_EPRINTF(LEVEL_ERR, "Unimplemented operation starting with token %s\n", tok_spelling(tok->type));
        break;
    }
}

void compile(const char *src, const char *file_path) {
    Lexer lexer;
    init_lexer(&lexer, src, file_path);

    Compiler compiler;
    init_compiler(&compiler, &lexer);

    lexer_next(&lexer);
    if (lexer.cur.type == TOK_EOF)
        eprintf(lexer.file_path, lexer.cur.line, lexer.cur.pos, LEVEL_WARN, "Empty file");

    while (lexer.cur.type != TOK_EOF)
        compile_stmt(&compiler);

    if (!compiler.had_error) {
        print_ops(&compiler.ops);
        generate_x86_64_linux(&compiler.ops);
    }
    free(compiler.ops.items);
}

