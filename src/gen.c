#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "compiler.h"
#include "utils.h"

void init_generator(Generator *gen, Ops *ops) {
    *gen = (Generator){0};
    gen->ops = ops;
}

void generate_x86_64_linux(Ops *ops, char *output_file, int gen_start) {
    Generator gen;
    init_generator(&gen, ops);

    if (gen_start) {
        sb_appendf(&gen.sb, ".globl _start\n");
        sb_appendf(&gen.sb, "_start:\n");
        sb_appendf(&gen.sb, "    call main\n");
        sb_appendf(&gen.sb, "    movq (%%rax), %%rdi\n");
        sb_appendf(&gen.sb, "    movq $60, %%rax\n");
        sb_appendf(&gen.sb, "    syscall\n");
    }

    Hash_Entry *func_entry = NULL;

    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        switch(op->opcode) {
        case OP_PUSH:
            if (op->operand > UINT32_MAX) {
                sb_appendf(&gen.sb, "    movabsq $%llu, %%rax\n", op->operand);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            else {
                sb_appendf(&gen.sb, "    pushq $%u\n", op->operand);
            }
            break;
        case OP_ADD:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    addq %%rax, (%%rsp)\n");
            break;
        case OP_SUB:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    subq %%rax, (%%rsp)\n");
            break;
        case OP_MUL:
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    imulq %%rbx, %%rax\n");
            sb_appendf(&gen.sb, "    pushq %%rax\n");
            break;
        case OP_DIV:
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    idivq %%rbx\n");
            sb_appendf(&gen.sb, "    xorq %%rdx, %%rdx\n");
            sb_appendf(&gen.sb, "    pushq %%rax\n");
            break;
        case OP_MOD:
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    idivq %%rbx\n");
            sb_appendf(&gen.sb, "    pushq %%rdx\n");
            break;
        case OP_AND:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    andq %%rax, (%%rsp)\n");
            break;
        case OP_OR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    orq %%rax, (%%rsp)\n");
            break;
        case OP_XOR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    xorq %%rax, (%%rsp)\n");
            break;
        case OP_SHL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    salq %%cl, (%%rsp)\n");
            break;
        case OP_SHR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    sarq %%cl, (%%rsp)\n");
            break;
        case OP_ROL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    rolq %%cl, (%%rsp)\n");
            break;
        case OP_ROR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    rorq %%cl, (%%rsp)\n");
            break;
        case OP_NOT:
            sb_appendf(&gen.sb, "    notq (%%rsp)\n");
            break;
        case OP_DUP:
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            break;
        case OP_OVER:
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            break;
        case OP_DUP2:
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            break;
        case OP_DROP:
            sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
            break;
        case OP_SWAP:
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rbx, 8(%%rsp)\n");
            break;
        case OP_OVER2:
            sb_appendf(&gen.sb, "    pushq 24(%%rsp)\n");
            sb_appendf(&gen.sb, "    pushq 24(%%rsp)\n");
            break;
        case OP_SWAP2:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rax, 24(%%rsp)\n");
            break;
        case OP_ROT:
            sb_appendf(&gen.sb, "    movq 8(%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq 16(%%rsp), %%rbx\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rbx, (%%rsp)\n");
            break;
        case OP_NEG:
            sb_appendf(&gen.sb, "    negq (%%rsp)\n");
            break;
        case OP_EQ:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    cmpq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    sete %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LT:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    cmpq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    setg %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GT:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    cmpq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    setl %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LTEQ:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    cmpq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    setge %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GTEQ:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    cmpq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    setle %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LNOT:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    testq %%rax, %%rax\n");
            sb_appendf(&gen.sb, "    setz %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    pushq %%rax\n");
            break;
        case OP_JMPF:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    testq %%rax, %%rax\n");
            sb_appendf(&gen.sb, "    jz .L%lld\n", op->operand);
            break;
        case OP_JMP:
            sb_appendf(&gen.sb, "    jmp .L%lld\n", op->operand);
            break;
        case OP_LABEL:
            sb_appendf(&gen.sb, ".L%lld:\n", op->operand);
            break;
        case OP_FUNC: {
            func_entry = (Hash_Entry *)op->operand;
            Function func = ((Symbol *)func_entry->val)->as.func;

            if (func.module_name.str == NULL) {
                sb_appendf(&gen.sb, ".globl \"%.*s\"\n", func_entry->key_len, func_entry->key);
                sb_appendf(&gen.sb, "\"%.*s\":\n", func_entry->key_len, func_entry->key);
            }
            else {
                sb_appendf(&gen.sb, ".globl \"%.*s::%.*s\"\n", func.module_name.len, func.module_name.str, func_entry->key_len, func_entry->key);
                sb_appendf(&gen.sb, "\"%.*s::%.*s\":\n", func.module_name.len, func.module_name.str, func_entry->key_len, func_entry->key);
            }
            sb_appendf(&gen.sb, "    pushq %%rbp\n");
            sb_appendf(&gen.sb, "    movq %%rsp, %%rbp\n");

            for (size_t offs = (func.arity-1)*8 + 16; offs >= 16; offs -= 8)
                sb_appendf(&gen.sb, "    pushq %zu(%%rbp)\n", offs);
            break;
        }
        case OP_RET: {
            sb_appendf(&gen.sb, "    movq %%rsp, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rbp, %%rsp\n");
            sb_appendf(&gen.sb, "    popq %%rbp\n");

            Function func = ((Symbol *)func_entry->val)->as.func;
            sb_appendf(&gen.sb, "    ret $%zu\n", func.arity*8);
            break;
        }
        case OP_CALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            Function func = ((Symbol *)entry->val)->as.func;

            if (func.module_name.str == NULL)
                sb_appendf(&gen.sb, "    call \"%.*s\"\n", entry->key_len, entry->key);
            else
                sb_appendf(&gen.sb, "    call \"%.*s::%.*s\"\n", func.module_name.len, func.module_name.str, entry->key_len, entry->key);

            for (int64_t offs = (func.ret_arity-1)*8; offs >= 0; offs -= 8)
                sb_appendf(&gen.sb, "    pushq %zu(%%rax)\n", offs);
            break;
        }
        case OP_STR:
            sb_appendf(&gen.sb, "    pushq $.str%zu\n", gen.strs.count);
            DA_APPEND(&gen.strs, (char *)op->operand);
            break;
        default:
            eprintf(op->file_path, op->line, op->pos, LEVEL_ERR, "Unimplemented instruction: ");
            print_op(op);
            gen.had_error = 1;
            break;
        }
    }

    sb_appendf(&gen.sb, ".section .rodata\n");
    for (size_t i = 0; i < gen.strs.count; i++)
        sb_appendf(&gen.sb, ".str%zu: .asciz \"%s\"\n", i, gen.strs.items[i]);

    if (gen.had_error) {
        free(gen.sb.items);
        return;
    }

    size_t len = strlen(output_file) + 3;
    char *output_asm = malloc(len);
    snprintf(output_asm, len, "%s.s", output_file);

    FILE *out = fopen(output_asm, "wb");
    if (!out) {
        fprintf(stderr, "\x1b[31mFailed to create output file\x1b[0m\n");
        free(gen.sb.items);
        return;
    }

    fwrite(gen.sb.items, sizeof(char), gen.sb.count, out);
    if (ferror(out)) {
        fprintf(stderr, "\x1b[31mFailed to write to output file\x1b[0m\n");
        free(gen.sb.items);
        return;
    }

    fclose(out);

    char *output_obj = malloc(len);
    snprintf(output_obj, len, "%s.o", output_file);

    Cmd cmd = {0};
    cmd_append_many(&cmd, 4, "as", "-o", output_obj, output_asm);
    if (!cmd_exec(&cmd)) return;

    free(cmd.items);
    free(gen.sb.items);
    free(output_asm);
    free(output_obj);
}

