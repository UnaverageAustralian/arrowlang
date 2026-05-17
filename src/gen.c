#include <stdio.h>
#include <stdlib.h>

#include "gen.h"
#include "compiler.h"
#include "utils.h"

void init_generator(Generator *gen, Ops *ops) {
    gen->ops = ops;
    gen->sb = (String_Builder){0};
    gen->had_error = 0;
}

void generate_x86_64_linux(Ops *ops) {
    Generator gen;
    init_generator(&gen, ops);

    sb_appendf(&gen.sb, "global _start\n");
    sb_appendf(&gen.sb, "_start:\n");
    sb_appendf(&gen.sb, "    call main\n");
    sb_appendf(&gen.sb, "    mov rdi, [rax]\n");
    sb_appendf(&gen.sb, "    mov rax, 60\n");
    sb_appendf(&gen.sb, "    syscall\n");

    Hash_Entry *func_entry = NULL;

    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];
        switch(op->opcode) {
        case OP_PUSH:
            if (op->operand > INT32_MAX || op->operand < INT32_MIN) {
                sb_appendf(&gen.sb, "    movabs rax, %lld\n", op->operand);
                sb_appendf(&gen.sb, "    push rax\n");
            }
            else {
                sb_appendf(&gen.sb, "    push %d\n", op->operand);
            }
            break;
        case OP_ADD:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    add qword [rsp], rbx\n");
            break;
        case OP_SUB:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    sub qword [rsp], rbx\n");
            break;
        case OP_MUL:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    imul rax, rbx\n");
            sb_appendf(&gen.sb, "    push rax\n");
            break;
        case OP_DIV:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    idiv rbx\n");
            sb_appendf(&gen.sb, "    push rax\n");
            break;
        case OP_MOD:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    idiv rbx\n");
            sb_appendf(&gen.sb, "    push rdx\n");
            break;
        case OP_AND:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    and qword [rsp], rbx\n");
            break;
        case OP_OR:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    or qword [rsp], rbx\n");
            break;
        case OP_XOR:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    xor qword [rsp], rbx\n");
            break;
        case OP_SHL:
            sb_appendf(&gen.sb, "    pop rcx\n");
            sb_appendf(&gen.sb, "    sal qword [rsp], cl\n");
            break;
        case OP_SHR:
            sb_appendf(&gen.sb, "    pop rcx\n");
            sb_appendf(&gen.sb, "    sar qword [rsp], cl\n");
            break;
        case OP_ROL:
            sb_appendf(&gen.sb, "    pop rcx\n");
            sb_appendf(&gen.sb, "    rol qword [rsp], cl\n");
            break;
        case OP_ROR:
            sb_appendf(&gen.sb, "    pop rcx\n");
            sb_appendf(&gen.sb, "    ror qword [rsp], cl\n");
            break;
        case OP_NOT:
            sb_appendf(&gen.sb, "    not qword [rsp]\n");
            break;
        case OP_DUP:
            sb_appendf(&gen.sb, "    push qword [rsp]\n");
            break;
        case OP_OVER:
            sb_appendf(&gen.sb, "    push qword [rsp+8]\n");
            break;
        case OP_DUP2:
            sb_appendf(&gen.sb, "    push qword [rsp+8]\n");
            sb_appendf(&gen.sb, "    push qword [rsp+8]\n");
            break;
        case OP_DROP:
            sb_appendf(&gen.sb, "    add rsp, 8\n");
            break;
        case OP_SWAP:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    push qword [rsp]\n");
            sb_appendf(&gen.sb, "    mov qword [rsp+8], rbx\n");
            break;
        case OP_OVER2:
            sb_appendf(&gen.sb, "    push qword [rsp+24]\n");
            sb_appendf(&gen.sb, "    push qword [rsp+24]\n");
            break;
        case OP_SWAP2:
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    push qword [rsp+8]\n");
            sb_appendf(&gen.sb, "    push qword [rsp+8]\n");
            sb_appendf(&gen.sb, "    mov qword [rsp+16], rax\n");
            sb_appendf(&gen.sb, "    mov qword [rsp+24], rbx\n");
            break;
        case OP_NEG:
            sb_appendf(&gen.sb, "    neg qword [rsp]\n");
            break;
        case OP_EQ:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    cmp rbx, qword [rsp]\n");
            sb_appendf(&gen.sb, "    sete bl\n");
            sb_appendf(&gen.sb, "    movzx rbx, bl\n");
            sb_appendf(&gen.sb, "    mov qword [rsp], rbx\n");
            break;
        case OP_LT:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    cmp rbx, qword [rsp]\n");
            sb_appendf(&gen.sb, "    setg bl\n");
            sb_appendf(&gen.sb, "    movzx rbx, bl\n");
            sb_appendf(&gen.sb, "    mov qword [rsp], rbx\n");
            break;
        case OP_GT:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    cmp rbx, qword [rsp]\n");
            sb_appendf(&gen.sb, "    setl bl\n");
            sb_appendf(&gen.sb, "    movzx rbx, bl\n");
            sb_appendf(&gen.sb, "    mov qword [rsp], rbx\n");
            break;
        case OP_LTEQ:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    cmp rbx, qword [rsp]\n");
            sb_appendf(&gen.sb, "    setge bl\n");
            sb_appendf(&gen.sb, "    movzx rbx, bl\n");
            sb_appendf(&gen.sb, "    mov qword [rsp], rbx\n");
            break;
        case OP_GTEQ:
            sb_appendf(&gen.sb, "    pop rbx\n");
            sb_appendf(&gen.sb, "    cmp rbx, qword [rsp]\n");
            sb_appendf(&gen.sb, "    setle bl\n");
            sb_appendf(&gen.sb, "    movzx rbx, bl\n");
            sb_appendf(&gen.sb, "    mov qword [rsp], rbx\n");
            break;
        case OP_LNOT:
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    test rax, rax\n");
            sb_appendf(&gen.sb, "    setz al\n");
            sb_appendf(&gen.sb, "    movzx rax, al\n");
            sb_appendf(&gen.sb, "    push rax\n");
            break;
        case OP_JMPF:
            sb_appendf(&gen.sb, "    pop rax\n");
            sb_appendf(&gen.sb, "    test rax, rax\n");
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
            sb_appendf(&gen.sb, "%.*s:\n", func_entry->key_len, func_entry->key);
            sb_appendf(&gen.sb, "    push rbp\n");
            sb_appendf(&gen.sb, "    mov rbp, rsp\n");

            Function *func = (Function *)func_entry->val;
            for (size_t offs = (func->arity-1)*8 + 16; offs >= 16; offs -= 8)
                sb_appendf(&gen.sb, "    push qword [rbp+%zu]\n", offs);
            break;
        }
        case OP_RET: {
            sb_appendf(&gen.sb, "    mov rax, rsp\n");
            sb_appendf(&gen.sb, "    mov rsp, rbp\n");
            sb_appendf(&gen.sb, "    pop rbp\n");

            Function *func = (Function *)func_entry->val;
            sb_appendf(&gen.sb, "    ret %zu\n", func->arity*8);
            break;
        }
        case OP_CALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            sb_appendf(&gen.sb, "    call %.*s\n", entry->key_len, entry->key);

            Function *func = (Function *)entry->val;
            for (int64_t offs = (func->ret_arity-1)*8; offs >= 0; offs -= 8)
                sb_appendf(&gen.sb, "    push qword [rax+%zu]\n", offs);
            break;
        }
        default:
            eprintf(op->file_path, op->line, op->pos, LEVEL_ERR, "Unimplemented instruction: ");
            print_op(op);
            gen.had_error = 1;
            break;
        }
    }

    if (gen.had_error) {
        free(gen.sb.items);
        return;
    }

    FILE *out = fopen("./a.s", "wb");
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

    free(gen.sb.items);
}

