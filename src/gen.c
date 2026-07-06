#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analyser.h"
#include "gen.h"
#include "compiler.h"
#include "utils.h"

#define GEN_EPRINTF(op, level, ...) eprintf(op->file_path, op->line, op->pos, level, __VA_ARGS__)

static char size_sufs[]  = ".bw.l...q";
static char fsize_sufs[] = "....s...d";
static char *rax[] = {"...", "%al", "%ax", "...", "%eax", "...", "...", "...", "%rax"};
static char *rbx[] = {"...", "%bl", "%bx", "...", "%ebx", "...", "...", "...", "%rbx"};

static char i8_i16[]  = "    movsbw (%rsp), %ax\n"
                        "    movw %ax, (%rsp)";
static char i8_i32[]  = "    movsbl (%rsp), %eax\n"
                        "    movl %eax, (%rsp)";
static char i8_i64[]  = "    movsbq (%rsp), %rax\n"
                        "    movq %rax, (%rsp)";
static char i16_i32[] = "    movswl (%rsp), %eax\n"
                        "    movl %eax, (%rsp)";
static char i16_i64[] = "    movswq (%rsp), %rax\n"
                        "    movq %rax, (%rsp)";
static char i32_i64[] = "    movslq (%rsp), %rax\n"
                        "    movq %rax, (%rsp)";
static char i16_i8[]  = "    movb (%rsp), %al\n"
                        "    movzbw %al, %ax\n"
                        "    movw %ax, (%rsp)";
static char i32_i8[]  = "    movb (%rsp), %al\n"
                        "    movzbl %al, %eax\n"
                        "    movl %eax, (%rsp)";
static char i64_i8[]  = "    movb (%rsp), %al\n"
                        "    movzbq %al, %rax\n"
                        "    movq %rax, (%rsp)";
static char i32_i16[] = "    movw (%rsp), %ax\n"
                        "    movzwl %ax, %eax\n"
                        "    movq %rax, (%rsp)";
static char i64_i32[] = "    movl (%rsp), %eax\n"
                        "    movq %rax, (%rsp)";

static char i64_f32[] = "    cvtsi2ssq (%rsp), %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char i64_f64[] = "    cvtsi2sdq (%rsp), %xmm0\n"
                        "    movsd %xmm0, (%rsp)";
static char i8_f32[]  = "    movsbl (%rsp), %eax\n"
                        "    cvtsi2ssl %eax, %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char i16_f32[] = "    movswl (%rsp), %eax\n"
                        "    cvtsi2ssl %eax, %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char i32_f32[] = "    cvtsi2ssl (%rsp), %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char i8_f64[]  = "    movsbl (%rsp), %eax\n"
                        "    cvtsi2sdl %eax, %xmm0\n"
                        "    movsd %xmm0, (%rsp)";
static char i16_f64[] = "    movswl (%rsp), %eax\n"
                        "    cvtsi2sdl %eax, %xmm0\n"
                        "    movsd %xmm0, (%rsp)";
static char i32_f64[] = "    cvtsi2sdl (%rsp), %xmm0\n"
                        "    movsd %xmm0, (%rsp)";

static char u8_f32[]  = "    movb (%rsp), %al\n"
                        "    movzbq %al, %rax\n"
                        "    cvtsi2ssq %rax, %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char u16_f32[] = "    movw (%rsp), %ax\n"
                        "    movzwq %ax, %rax\n"
                        "    cvtsi2ssq %rax, %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char u32_f32[] = "    movl (%rsp), %eax\n"
                        "    cvtsi2ssq %rax, %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char u8_f64[]  = "    movb (%rsp), %al\n"
                        "    movzbq %al, %rax\n"
                        "    cvtsi2sdq %rax, %xmm0\n"
                        "    movsd %xmm0, (%rsp)";
static char u16_f64[] = "    movw (%rsp), %ax\n"
                        "    movzwq %ax, %rax\n"
                        "    cvtsi2sdq %rax, %xmm0\n"
                        "    movsd %xmm0, (%rsp)";
static char u32_f64[] = "    movl (%rsp), %eax\n"
                        "    cvtsi2sdq %rax, %xmm0\n"
                        "    movsd %xmm0, (%rsp)";

static char f64_i64[] = "    cvttsd2siq (%rsp), %rax\n"
                        "    movq %rax, (%rsp)";
static char f64_i32[] = "    cvttsd2sil (%rsp), %eax\n"
                        "    movq %rax, (%rsp)";
static char f64_i16[] = "    cvttsd2sil (%rsp), %eax\n"
                        "    movzwq %ax, %rax\n"
                        "    movq %rax, (%rsp)";
static char f64_i8[]  = "    cvttsd2sil (%rsp), %eax\n"
                        "    movzbq %al, %rax\n"
                        "    movq %rax, (%rsp)";
static char f32_i64[] = "    cvttss2siq (%rsp), %rax\n"
                        "    movq %rax, (%rsp)";
static char f32_i32[] = "    cvttss2sil (%rsp), %eax\n"
                        "    movq %rax, (%rsp)";
static char f32_i16[] = "    cvttss2sil (%rsp), %eax\n"
                        "    movzwq %ax, %rax\n"
                        "    movq %rax, (%rsp)";
static char f32_i8[]  = "    cvttss2sil (%rsp), %eax\n"
                        "    movzbq %al, %rax\n"
                        "    movq %rax, (%rsp)";
static char f64_f32[] = "    pxor %xmm0, %xmm0\n"
                        "    cvtsd2ss (%rsp), %xmm0\n"
                        "    movss %xmm0, (%rsp)";
static char f32_f64[] = "    pxor %xmm0, %xmm0\n"
                        "    cvtss2sd (%rsp), %xmm0\n"
                        "    movsd %xmm0, (%rsp)";

static char *conv_table[12][12] = {
    { NULL,   NULL,   NULL,   i8_i16,  NULL,    i8_i32,  NULL,    i8_i64,  NULL,    i8_f32,  i8_f64,  NULL },
    { NULL,   NULL,   NULL,   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u8_f32,  u8_f64,  NULL },
    { NULL,   NULL,   NULL,   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u8_f32,  u8_f64,  NULL },
    { i16_i8, i16_i8, i16_i8, NULL,    NULL,    i16_i32, NULL,    i16_i64, NULL,    i16_f32, i16_f64, NULL },
    { i16_i8, i16_i8, i16_i8, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u16_f32, u16_f64, NULL },
    { i32_i8, i32_i8, i32_i8, i32_i16, i32_i16, NULL,    NULL,    i32_i64, NULL,    i32_f32, i32_f64, NULL },
    { i32_i8, i32_i8, i32_i8, i32_i16, i32_i16, NULL,    NULL,    NULL,    NULL,    u32_f32, u32_f64, NULL },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    i64_f32, i64_f64, NULL },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    i64_f32, i64_f64, NULL },
    { f32_i8, f32_i8, f32_i8, f32_i16, f32_i16, f32_i32, f32_i32, f32_i64, f32_i64, NULL,    f32_f64, NULL },
    { f64_i8, f64_i8, f64_i8, f64_i16, f64_i16, f64_i32, f64_i32, f64_i64, f64_i64, f64_f32, NULL,    NULL },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    NULL,    NULL,    NULL },
};

static char *arg_regs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

void init_generator(Generator *gen, Ops *ops) {
    *gen = (Generator){0};
    gen->ops = ops;
}

static int size(Type type) {
    switch (type.kind) {
    case KIND_BASIC:
        switch (type.as.basic) {
        case TYPE_I8:
        case TYPE_CHAR:
        case TYPE_U8:
            return 1;
        case TYPE_I16:
        case TYPE_U16:
            return 2;
        case TYPE_F32:
        case TYPE_I32:
        case TYPE_U32:
            return 4;
        case TYPE_STR:
        case TYPE_F64:
        case TYPE_I64:
        case TYPE_U64:
            return 8;
        default:
            return 0;
        }
    case KIND_STRUCT:
        return type.as.structure.size;
    }
    return 0;
}

int typeid(Type type) {
    switch (type.as.basic) {
    case TYPE_I8:   return 0;
    case TYPE_CHAR: return 1;
    case TYPE_U8:   return 2;
    case TYPE_I16:  return 3;
    case TYPE_U16:  return 4;
    case TYPE_I32:  return 5;
    case TYPE_U32:  return 6;
    case TYPE_INTEGER:
    case TYPE_I64:  return 7;
    case TYPE_U64:  return 8;
    case TYPE_F32:  return 9;
    case TYPE_REAL:
    case TYPE_F64:  return 10;
    case TYPE_STR:  return 11;
    default:        return 0;
    }
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

    Function cur_func = {0};

    for (size_t i = 0; i < ops->count; i++) {
        Op *op = &ops->items[i];

        switch(op->opcode) {
        case OP_PUSH:
            if (op->operand > UINT32_MAX) {
                sb_appendf(&gen.sb, "    movabsq $%llu, %%rax\n", op->operand);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            else if (op->operand > INT32_MAX) {
                sb_appendf(&gen.sb, "    movl $%u, %%eax\n", op->operand);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            else {
                sb_appendf(&gen.sb, "    pushq $%d\n", op->operand);
            }
            break;
        case OP_ADD:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    adds%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    add%c %s, (%%rsp)\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            }
            break;
        case OP_SUB:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    subs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    sub%c %s, (%%rsp)\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            }
            break;
        case OP_MUL:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    muls%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rbx\n");
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    imul%c %s\n", size_sufs[size(op->types[0])], rbx[size(op->types[0])]);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            break;
        case OP_DIV:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    divs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rbx\n");
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    xorq %%rdx, %%rdx\n");
                sb_appendf(&gen.sb, "    idiv%c %s\n", size_sufs[size(op->types[0])], rbx[size(op->types[0])]);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            break;
        case OP_MOD:
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    xorq %%rdx, %%rdx\n");
            sb_appendf(&gen.sb, "    idiv%c %s\n", size_sufs[size(op->types[0])], rbx[size(op->types[0])]);
            sb_appendf(&gen.sb, "    pushq %%rdx\n");
            break;
        case OP_AND:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    and%c %s, (%%rsp)\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            break;
        case OP_OR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    or%c %s, (%%rsp)\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            break;
        case OP_XOR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    xor%c %s, (%%rsp)\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            break;
        case OP_SHL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    sal%c %%cl, (%%rsp)\n", size_sufs[size(op->types[0])]);
            break;
        case OP_SHR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    sar%c %%cl, (%%rsp)\n", size_sufs[size(op->types[0])]);
            break;
        case OP_ROL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    rol%c %%cl, (%%rsp)\n", size_sufs[size(op->types[0])]);
            break;
        case OP_ROR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    ror%c %%cl, (%%rsp)\n", size_sufs[size(op->types[0])]);
            break;
        case OP_NOT:
            sb_appendf(&gen.sb, "    not%c (%%rsp)\n", size_sufs[size(op->types[0])]);
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
            if (op->types[0].kind == KIND_STRUCT)
                cur_func.allocated -= op->types[0].as.structure.size;
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
            sb_appendf(&gen.sb, "    movq %%rbx, 24(%%rsp)\n");
            break;
        case OP_ROT:
            sb_appendf(&gen.sb, "    movq 8(%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq 16(%%rsp), %%rbx\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rbx, (%%rsp)\n");
            break;
        case OP_ROTN:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    movq 8(%%rsp), %%rbx\n");
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rbx, 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            break;
        case OP_NEG:
            if (op->types[0].as.basic == TYPE_F32) {
                sb_appendf(&gen.sb, "    xorl $0x80000000, (%%rsp)\n");
            }
            else if (op->types[0].as.basic == TYPE_F64) {
                sb_appendf(&gen.sb, "    movabsq $0x8000000000000000, %%rax\n");
                sb_appendf(&gen.sb, "    xorq %%rax, (%%rsp)\n");
            }
            else {
                sb_appendf(&gen.sb, "    neg%c (%%rsp)\n", size_sufs[size(op->types[0])]);
            }
            break;
        case OP_EQ:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
            }
            sb_appendf(&gen.sb, "    sete %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LT:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    seta %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setg %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GT:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setb %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setl %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LTEQ:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setae %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setge %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GTEQ:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setbe %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setle %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_NEQ:
            if (op->types[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setne %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])]);
                sb_appendf(&gen.sb, "    setne %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LNOT:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    test%c %s, %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])], rax[size(op->types[0])]);
            sb_appendf(&gen.sb, "    setz %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    pushq %%rax\n");
            break;
        case OP_JMPF:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    test%c %s, %s\n", size_sufs[size(op->types[0])], rax[size(op->types[0])], rax[size(op->types[0])]);
            sb_appendf(&gen.sb, "    jz .L%lld\n", op->operand);
            break;
        case OP_JMP:
            sb_appendf(&gen.sb, "    jmp .L%lld\n", op->operand);
            break;
        case OP_LABEL:
            sb_appendf(&gen.sb, ".L%lld:\n", op->operand);
            break;
        case OP_FUNC: {
            Hash_Entry *func_entry = (Hash_Entry *)op->operand;
            cur_func = ((Symbol *)func_entry->val)->as.func;

            if (cur_func.module_name.str == NULL) {
                sb_appendf(&gen.sb, ".globl \"%.*s\"\n", func_entry->key_len, func_entry->key);
                sb_appendf(&gen.sb, "\"%.*s\":\n", func_entry->key_len, func_entry->key);
            }
            else {
                sb_appendf(&gen.sb, ".globl \"%.*s::%.*s\"\n", cur_func.module_name.len, cur_func.module_name.str, func_entry->key_len, func_entry->key);
                sb_appendf(&gen.sb, "\"%.*s::%.*s\":\n", cur_func.module_name.len, cur_func.module_name.str, func_entry->key_len, func_entry->key);
            }
            sb_appendf(&gen.sb, "    pushq %%rbp\n");
            sb_appendf(&gen.sb, "    movq %%rsp, %%rbp\n");
            sb_appendf(&gen.sb, "    subq $%d, %%rsp\n", cur_func.max_allocated);

            for (size_t offs = (cur_func.param_types.count-1)*8 + 16; offs >= 16; offs -= 8)
                sb_appendf(&gen.sb, "    pushq %zu(%%rbp)\n", offs);
            break;
        }
        case OP_RET: {
            sb_appendf(&gen.sb, "    movq %%rsp, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rbp, %%rsp\n");
            sb_appendf(&gen.sb, "    popq %%rbp\n");

            Function func = cur_func;
            sb_appendf(&gen.sb, "    ret $%zu\n", func.param_types.count*8);
            break;
        }
        case OP_CALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            Function func = ((Symbol *)entry->val)->as.func;

            if (func.module_name.str == NULL)
                sb_appendf(&gen.sb, "    call \"%.*s\"\n", entry->key_len, entry->key);
            else
                sb_appendf(&gen.sb, "    call \"%.*s::%.*s\"\n", func.module_name.len, func.module_name.str, entry->key_len, entry->key);

            for (int64_t offs = (func.return_types.count-1)*8; offs >= 0; offs -= 8)
                sb_appendf(&gen.sb, "    pushq %zu(%%rax)\n", offs);
            break;
        }
        case OP_CCALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            Function func = ((Symbol *)entry->val)->as.func;

            int iparams = 0;
            int fparams = 0;
            for (size_t i = 0; i < func.param_types.count; i++) {
                if ((func.param_types.items[i].as.basic & TYPE_REAL) && fparams < 8)
                    fparams++;
                else if (iparams < 6)
                    iparams++;
            }

            for (int i = func.param_types.count-1; i >= 0; i--) {
                if ((func.param_types.items[i].as.basic & TYPE_REAL) && fparams >= 0) {
                    sb_appendf(&gen.sb, "    movsd (%%rsp), %%xmm%d\n", fparams-1);
                    sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                    fparams--;
                }
                else if (iparams >= 0) {
                    sb_appendf(&gen.sb, "    popq %s\n", arg_regs[iparams-1]);
                    iparams--;
                }
            }

            sb_appendf(&gen.sb, "    call \"%.*s\"\n", entry->key_len, entry->key);

            if (func.return_types.count == 1 && func.return_types.items[0].as.basic & TYPE_REAL) {
                sb_appendf(&gen.sb, "    subq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movsd %%xmm0, (%%rsp)\n");
            }
            else if (func.return_types.count == 1) {
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            else if (func.return_types.count != 0) {
                GEN_EPRINTF(op, LEVEL_ERR, "Calling C functions with more than one return value is not implemented\n");
            }
            break;
        }
        case OP_STR:
            sb_appendf(&gen.sb, "    pushq $.str%zu\n", gen.strs.count);
            DA_APPEND(&gen.strs, (char *)op->operand);
            break;
        case OP_CONVERT: {
            char *conv_str = conv_table[typeid(op->types[0])][typeid(op->types[1])];
            if (conv_str == NULL) break;

            if (op->operand != 0) {
                sb_appendf(&gen.sb, "    addq $%llu, %%rsp\n", op->operand);
                sb_appendf(&gen.sb, "%s\n", conv_str);
                sb_appendf(&gen.sb, "    subq $%llu, %%rsp\n", op->operand);
            }
            else {
                sb_appendf(&gen.sb, "%s\n", conv_str);
            }
            break;
        }
        case OP_INIT: {
            Struct structure = op->types[0].as.structure;
            // int start_offs = size(op->types[0].as.structure.fields.items[0].type) + structure.size;

            sb_appendf(&gen.sb, "    leaq %d(%%rbp), %%rdi\n", cur_func.allocated - cur_func.max_allocated);
            cur_func.allocated += structure.size;

            sb_appendf(&gen.sb, "    addq $%lld, %%rsp\n", structure.fields.count*8);
            for (size_t i = 0; i < structure.fields.count; i++) {
                Field *field = &structure.fields.items[i];

                if (field->type.kind == KIND_STRUCT) {
                    sb_appendf(&gen.sb, "    movq %d(%%rsp), %%rsi\n", -i*8 - 8);
                    for (int j = 0; j < field->type.as.structure.size; j += 8) {
                        sb_appendf(&gen.sb, "    movq %d(%%rsi), %%rax\n", j);
                        sb_appendf(&gen.sb, "    movq %%rax, %d(%%rdi)\n", field->offset + j);
                    }
                }
                else {
                    sb_appendf(&gen.sb, "    mov%c %lld(%%rsp), %s\n", size_sufs[size(field->type)], -i*8 - 8, rax[size(field->type)]);
                    sb_appendf(&gen.sb, "    mov%c %s, %lld(%%rdi)\n", size_sufs[size(field->type)], rax[size(field->type)], field->offset);
                }
            }
            sb_appendf(&gen.sb, "    pushq %%rdi\n");
            break;
        }
        case OP_NOP: break;
        default:
            GEN_EPRINTF(op, LEVEL_ERR, "Unimplemented instruction: %s\n", opcode_spelling(op->opcode));
            gen.had_error = 1;
            break;
        }
    }

    if (gen.strs.count > 0)
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

