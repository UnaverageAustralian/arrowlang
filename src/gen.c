#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "compiler.h"
#include "types.h"
#include "utils.h"

#define GEN_EPRINTF(op, level, ...) eprintf((op)->file_path, (op)->loc, level, __VA_ARGS__)

static char size_sufs[]  = ".bw.l...q";
static char csize_sufs[] = ".bw.d...q";
static char fsize_sufs[] = "....s...d";
static char rsize_sufs[] = ".bw.d... ";
static char *rax[] = {"...", "%al", "%ax", "...", "%eax", "...", "...", "...", "%rax"};
static char *rdx[] = {"...", "%dl", "%dx", "...", "%edx", "...", "...", "...", "%rdx"};

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
    { NULL,   NULL,   NULL,   i8_i16,  NULL,    i8_i32,  NULL,    i8_i64,  NULL,    i8_f32,  i8_f64,  },
    { NULL,   NULL,   NULL,   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u8_f32,  u8_f64,  },
    { NULL,   NULL,   NULL,   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u8_f32,  u8_f64,  },
    { i16_i8, i16_i8, i16_i8, NULL,    NULL,    i16_i32, NULL,    i16_i64, NULL,    i16_f32, i16_f64, },
    { i16_i8, i16_i8, i16_i8, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    u16_f32, u16_f64, },
    { i32_i8, i32_i8, i32_i8, i32_i16, i32_i16, NULL,    NULL,    i32_i64, NULL,    i32_f32, i32_f64, },
    { i32_i8, i32_i8, i32_i8, i32_i16, i32_i16, NULL,    NULL,    NULL,    NULL,    u32_f32, u32_f64, },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    i64_f32, i64_f64, },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    i64_f32, i64_f64, },
    { f32_i8, f32_i8, f32_i8, f32_i16, f32_i16, f32_i32, f32_i32, f32_i64, f32_i64, NULL,    f32_f64, },
    { f64_i8, f64_i8, f64_i8, f64_i16, f64_i16, f64_i32, f64_i32, f64_i64, f64_i64, f64_f32, NULL,    },
    { i64_i8, i64_i8, i64_i8, i32_i16, i32_i16, i64_i32, i64_i32, NULL,    NULL,    NULL,    NULL,    },
};

static char *arg_regs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

void init_generator(Generator *gen, Ops *ops) {
    *gen = (Generator){0};
    gen->ops = ops;
}

void move_struct(Generator *gen, Struct a, int offs) {
    for (int i = 0; i < a.size - (a.size & 7); i += 8) {
        sb_appendf(&gen->sb, "    movq %d(%%rsi), %%rax\n", i);
        sb_appendf(&gen->sb, "    movq %%rax, %d(%%rdi)\n", i + offs);
    }

    if ((a.size & 7) == 0) return;

    uint64_t c = 8;
    while ((a.size & c) == 0)
        c >>= 1;

    int piece_size = a.size & c;
    for (int i = a.size - (a.size & 7); i < a.size; i += piece_size) {
        if (i + piece_size > a.size)
            i = a.size - piece_size;
        sb_appendf(&gen->sb, "    mov%c %d(%%rsi), %s\n", size_sufs[piece_size], i, rax[piece_size]);
        sb_appendf(&gen->sb, "    mov%c %s, %d(%%rdi)\n", size_sufs[piece_size], rax[piece_size], i + offs);
    }
}

void duplicate_struct(Generator *gen, Struct a) {
    sb_appendf(&gen->sb, "    popq %%rsi\n");
    sb_appendf(&gen->sb, "    leaq %d(%%rbp), %%rdi\n", gen->allocated - gen->func.max_allocated);
    move_struct(gen, a, 0);
    sb_appendf(&gen->sb, "    pushq %%rdi\n");
    gen->allocated += a.size;
}

void generate_ccall(Generator *gen, Hash_Entry *entry) {
    Function func = ((Symbol *)entry->val)->as.func;

    int extra_arg = 0;

    int iparams = 0;
    int fparams = 0;
    int sparams = 0;

    for (size_t i = 0; i < func.param_types.count; i++) {
        Type *param = &func.param_types.items[i];
        if (IS_ADVANCED(*param) && param->advanced->structure.size <= 16) {
            int first_is_float = first_long_is_float(param->advanced->structure);
            int last_is_float = last_long_is_float(param->advanced->structure);

            if (first_is_float)
                fparams++;
            else
                iparams++;

            if (last_is_float)
                fparams++;
            else
                iparams++;
        }
        else if (IS_REAL(*param)) {
            fparams++;
        }
        else if (!IS_ADVANCED(*param)) {
            iparams++;
        }
        else {
            sparams++;
        }
    }

    iparams = 0;
    fparams = 0;

    if (func.return_types.count == 1 && IS_ADVANCED(func.return_types.items[0]) && func.return_types.items[0].advanced->structure.size > 16) {
        iparams++;
        extra_arg = 1;
        sb_appendf(&gen->sb, "    leaq %d(%%rbp), %%rdi\n", gen->allocated - gen->func.max_allocated);
    }

    sb_appendf(&gen->sb, "    movq %%rsp, %%rbx\n");
    sb_appendf(&gen->sb, "    and $0xFFF0, %%sp\n");
    if (sparams & 1)
        sb_appendf(&gen->sb, "    subq $8, %%rsp\n");

    for (size_t i = 0; i < func.param_types.count; i++) {
        Type *param = &func.param_types.items[i];
        if (IS_ADVANCED(*param)) {
            sb_appendf(&gen->sb, "    movq %d(%%rbx), %%rax\n", (func.param_types.count - i)*8 - 8);

            int first_is_float = first_long_is_float(param->advanced->structure);
            int last_is_float = last_long_is_float(param->advanced->structure);
            int needed_fparams = first_is_float + last_is_float;

            int size = param->advanced->structure.size;
            if (size <= 16 && fparams + needed_fparams < 8 && iparams + (ALIGN(size, 8)/8 - needed_fparams) < 6) {
                if (first_is_float) {
                    sb_appendf(&gen->sb, "    movsd (%%rax), %%xmm%d\n", fparams);
                    fparams++;
                }
                else {
                    sb_appendf(&gen->sb, "    movq (%%rax), %s\n", arg_regs[iparams]);
                    iparams++;
                }

                if (last_is_float) {
                    sb_appendf(&gen->sb, "    movsd 8(%%rax), %%xmm%d\n", fparams);
                    fparams++;
                }
                else {
                    sb_appendf(&gen->sb, "    movq 8(%%rax), %s\n", arg_regs[iparams]);
                    iparams++;
                }
            }
            else {
                sb_appendf(&gen->sb, "    subq $%d, %%rsp\n", ALIGN(param->advanced->structure.size, 8));
                for (int i = 0; i < param->advanced->structure.size; i += 8) {
                    sb_appendf(&gen->sb, "    movq %d(%%rax), %%r11\n", i);
                    sb_appendf(&gen->sb, "    movq %%r11, %d(%%rsp)\n", i);
                }
            }
        }
        else if (IS_REAL(*param) && fparams < 8) {
            sb_appendf(&gen->sb, "    movsd %d(%%rbx), %%xmm%d\n", (func.param_types.count - i)*8 - 8, fparams);
            fparams++;
        }
        else if (iparams < 6) {
            sb_appendf(&gen->sb, "    movq %d(%%rbx), %s\n", (func.param_types.count - i)*8 - 8, arg_regs[iparams]);
            iparams++;
        }
    }

    sb_appendf(&gen->sb, "    call \"%.*s\"\n", func.extern_name.len, func.extern_name.str);
    sb_appendf(&gen->sb, "    movq %%rbx, %%rsp\n");
    if (func.param_types.count > 0)
        sb_appendf(&gen->sb, "    addq $%d, %%rsp\n", func.param_types.count*8);

    if (extra_arg) {
        sb_appendf(&gen->sb, "    leaq %d(%%rbp), %%rax\n", gen->allocated - gen->func.max_allocated);
        sb_appendf(&gen->sb, "    pushq %%rax\n");
        gen->allocated += func.return_types.items[0].advanced->structure.size;
    }

    if (func.return_types.count == 1 && IS_ADVANCED(func.return_types.items[0])) {
        Struct structure = func.return_types.items[0].advanced->structure;
        if (structure.size > 16) return;

        int first_is_float = first_long_is_float(structure);
        int last_is_float = last_long_is_float(structure);

        if (first_is_float) {
            sb_appendf(&gen->sb, "    subq $8, %%rsp\n");
            sb_appendf(&gen->sb, "    movsd %%xmm0, (%%rsp)\n");
        }
        else {
            sb_appendf(&gen->sb, "    movq %%rax, %d(%%rbp)\n", gen->allocated - gen->func.max_allocated);
        }

        if (last_is_float) {
            sb_appendf(&gen->sb, "    subq $8, %%rsp\n");
            sb_appendf(&gen->sb, "    movsd %%xmm%d, (%%rsp)\n", first_is_float ? 1 : 0);
        }
        else {
            sb_appendf(&gen->sb, "    movq %s, %d(%%rbp)\n", first_is_float ? "%rax" : "%rdx", gen->allocated - gen->func.max_allocated + 8);
        }

        gen->allocated += structure.size;
    }
    else if (func.return_types.count == 1 && IS_REAL(func.return_types.items[0])) {
        sb_appendf(&gen->sb, "    subq $8, %%rsp\n");
        sb_appendf(&gen->sb, "    movsd %%xmm0, (%%rsp)\n");
    }
    else if (func.return_types.count == 1) {
        sb_appendf(&gen->sb, "    pushq %%rax\n");
    }
    else if (func.return_types.count != 0) {
        GEN_EPRINTF(&gen->ops->items[gen->pos], LEVEL_ERR, "Calling C functions with more than one return value is not implemented\n");
    }
}

char *generate_x86_64(Ops *ops, char *output_file, int gen_start) {
    Generator gen;
    init_generator(&gen, ops);

    if (gen_start) {
        sb_appendf(&gen.sb, ".globl _start\n");
        sb_appendf(&gen.sb, "_start:\n");
        sb_appendf(&gen.sb, "    call \"main\"\n");
        sb_appendf(&gen.sb, "    movq (%%rax), %%rbx\n");
        sb_appendf(&gen.sb, "    call \"io::flush\"\n");
        sb_appendf(&gen.sb, "    movq %%rbx, %%rdi\n");
        sb_appendf(&gen.sb, "    movq $60, %%rax\n");
        sb_appendf(&gen.sb, "    syscall\n");
    }

    while (gen.pos < ops->count) {
        Op *op = &ops->items[gen.pos];

        sb_appendf(&gen.sb, "# %s\n", opcode_spelling(op->opcode));
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
        case OP_ADD: {
            Type deref = deref_type(op->types[0]);
            if (op->types[0].kind == TYPE_PTR && type_size(deref) > 1) {
                int size = type_size(deref);
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    imulq $%d, %s, %s\n", size, op->operand == 8 ? "(%rsp)" : "%rax", op->operand == 8 ? "%rdx" : "%rax");
                sb_appendf(&gen.sb, "    addq %%rax, %s\n", op->operand == 8 ? "%rdx" : "(%rsp)");
                if (op->operand == 8)
                    sb_appendf(&gen.sb, "    movq %%rdx, (%%rsp)\n");
            }
            else if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    adds%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[type_size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    add%c %s, (%%rsp)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            }
            break;
        }
        case OP_SUB: {
            Type deref = deref_type(op->types[0]);
            if (op->types[0].kind == TYPE_PTR && op->types[1].kind != TYPE_PTR && type_size(deref) > 1) {
                int size = type_size(deref);
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    imulq $%d, %s, %s\n", size, op->operand == 8 ? "(%rsp)" : "%rax", op->operand == 8 ? "%rdx" : "%rax");
                sb_appendf(&gen.sb, "    subq %%rax, %s\n", op->operand == 8 ? "%rdx" : "(%rsp)");
                if (op->operand == 8)
                    sb_appendf(&gen.sb, "    movq %%rdx, (%%rsp)\n");
            }
            else if (op->types[1].kind == TYPE_PTR && type_size(deref_type(op->types[1])) > 1) {
                int size = type_size(deref);
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    popq %%rcx\n");
                sb_appendf(&gen.sb, "    subq %%rcx, %%rax\n");
                sb_appendf(&gen.sb, "    movq $%d, %%rcx\n", size);
                sb_appendf(&gen.sb, "    idivq %%rcx\n");
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            else if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    subs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[type_size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    sub%c %s, (%%rsp)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            }
            break;
        }
        case OP_MUL:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    muls%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[type_size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rdx\n");
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    imul%c %s\n", size_sufs[type_size(op->types[0])], rdx[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            break;
        case OP_DIV:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c 8(%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    divs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    movs%c %%xmm0, (%%rsp)\n", fsize_sufs[type_size(op->types[0])]);
            }
            else {
                int size = type_size(op->types[0]);
                sb_appendf(&gen.sb, "    popq %%r8\n");
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    c%c%c\n", csize_sufs[size], csize_sufs[size] == 'q' ? 'o' : csize_sufs[size * 2]);
                sb_appendf(&gen.sb, "    idiv%c %%r8%c\n", size_sufs[size], rsize_sufs[size]);
                sb_appendf(&gen.sb, "    pushq %%rax\n");
            }
            break;
        case OP_MOD: {
            int size = type_size(op->types[0]);
            sb_appendf(&gen.sb, "    popq %%r8\n");
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    c%c%c\n", csize_sufs[size], csize_sufs[size] == 'q' ? 'o' : csize_sufs[size * 2]);
            sb_appendf(&gen.sb, "    idiv%c %%r8%c\n", size_sufs[size], rsize_sufs[size]);
            sb_appendf(&gen.sb, "    pushq %%rdx\n");
            break;
        }
        case OP_AND:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    and%c %s, (%%rsp)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            break;
        case OP_OR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    or%c %s, (%%rsp)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            break;
        case OP_XOR:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    xor%c %s, (%%rsp)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            break;
        case OP_SHL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    sal%c %%cl, (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            break;
        case OP_SHR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    sar%c %%cl, (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            break;
        case OP_ROL:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    rol%c %%cl, (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            break;
        case OP_ROR:
            sb_appendf(&gen.sb, "    popq %%rcx\n");
            sb_appendf(&gen.sb, "    ror%c %%cl, (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            break;
        case OP_NOT:
            sb_appendf(&gen.sb, "    not%c (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            break;
        case OP_DUP:
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            if (IS_ADVANCED(op->types[0]))
                duplicate_struct(&gen, op->types[0].advanced->structure);
            break;
        case OP_OVER:
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            if (IS_ADVANCED(op->types[0]))
                duplicate_struct(&gen, op->types[0].advanced->structure);
            break;
        case OP_DUP2:
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            if (IS_ADVANCED(op->types[0]))
                duplicate_struct(&gen, op->types[0].advanced->structure);

            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            if (IS_ADVANCED(op->types[1]))
                duplicate_struct(&gen, op->types[1].advanced->structure);
            break;
        case OP_DROP:
            sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
            break;
        case OP_SWAP:
            sb_appendf(&gen.sb, "    popq %%rdx\n");
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rdx, 8(%%rsp)\n");
            break;
        case OP_OVER2:
            sb_appendf(&gen.sb, "    pushq 24(%%rsp)\n");
            if (IS_ADVANCED(op->types[0]))
                duplicate_struct(&gen, op->types[0].advanced->structure);

            sb_appendf(&gen.sb, "    pushq 24(%%rsp)\n");
            if (IS_ADVANCED(op->types[1]))
                duplicate_struct(&gen, op->types[1].advanced->structure);
            break;
        case OP_SWAP2:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    popq %%rdx\n");
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    pushq 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rdx, 24(%%rsp)\n");
            break;
        case OP_ROT:
            sb_appendf(&gen.sb, "    movq 8(%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq 16(%%rsp), %%rdx\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rdx, (%%rsp)\n");
            break;
        case OP_ROTN:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    movq 8(%%rsp), %%rdx\n");
            sb_appendf(&gen.sb, "    pushq (%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rdx, 8(%%rsp)\n");
            sb_appendf(&gen.sb, "    movq %%rax, 16(%%rsp)\n");
            break;
        case OP_LDROP:
            sb_appendf(&gen.sb, "    popq (%%rsp)\n");
            break;
        case OP_NEG:
            if (op->types[0].kind == TYPE_F32) {
                sb_appendf(&gen.sb, "    xorl $0x80000000, (%%rsp)\n");
            }
            else if (op->types[0].kind == TYPE_F64) {
                sb_appendf(&gen.sb, "    movabsq $0x8000000000000000, %%rax\n");
                sb_appendf(&gen.sb, "    xorq %%rax, (%%rsp)\n");
            }
            else {
                sb_appendf(&gen.sb, "    neg%c (%%rsp)\n", size_sufs[type_size(op->types[0])]);
            }
            break;
        case OP_EQ:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            }
            sb_appendf(&gen.sb, "    sete %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LT:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    seta %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setg %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GT:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setb %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setl %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LTEQ:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setae %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setge %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_GTEQ:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setbe %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setle %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_NEQ:
            if (IS_REAL(op->types[0])) {
                sb_appendf(&gen.sb, "    movs%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    addq $8, %%rsp\n");
                sb_appendf(&gen.sb, "    ucomis%c (%%rsp), %%xmm0\n", fsize_sufs[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setne %%al\n");
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rax\n");
                sb_appendf(&gen.sb, "    cmp%c (%%rsp), %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
                sb_appendf(&gen.sb, "    setne %%al\n");
            }
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rax, (%%rsp)\n");
            break;
        case OP_LNOT:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    test%c %s, %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])], rax[type_size(op->types[0])]);
            sb_appendf(&gen.sb, "    setz %%al\n");
            sb_appendf(&gen.sb, "    movzbq %%al, %%rax\n");
            sb_appendf(&gen.sb, "    pushq %%rax\n");
            break;
        case OP_JMPF:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    test%c %s, %s\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])], rax[type_size(op->types[0])]);
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
            gen.func = ((Symbol *)func_entry->val)->as.func;

            if (gen.func.module_name.str == NULL) {
                sb_appendf(&gen.sb, ".globl \"%.*s\"\n", gen.func.extern_name.len, gen.func.extern_name.str);
                sb_appendf(&gen.sb, "\"%.*s\":\n", gen.func.extern_name.len, gen.func.extern_name.str);
            }
            else {
                sb_appendf(&gen.sb, ".globl \"%.*s::%.*s\"\n", gen.func.module_name.len, gen.func.module_name.str,
                           gen.func.extern_name.len, gen.func.extern_name.str);
                sb_appendf(&gen.sb, "\"%.*s::%.*s\":\n", gen.func.module_name.len, gen.func.module_name.str,
                           gen.func.extern_name.len, gen.func.extern_name.str);
            }
            sb_appendf(&gen.sb, "    pushq %%rbp\n");
            sb_appendf(&gen.sb, "    pushq %%rbx\n");
            sb_appendf(&gen.sb, "    movq %%rsp, %%rbp\n");
            sb_appendf(&gen.sb, "    subq $%d, %%rsp\n", gen.func.max_allocated);

            for (size_t i = 0; i < gen.func.param_types.count; i++) {
                sb_appendf(&gen.sb, "    pushq %zu(%%rbp)\n", (gen.func.param_types.count-i-1)*8 + 24);
                if (IS_ADVANCED(gen.func.param_types.items[i]))
                    duplicate_struct(&gen, gen.func.param_types.items[i].advanced->structure);
            }
            break;
        }
        case OP_RET: {
            sb_appendf(&gen.sb, "    movq %%rsp, %%rax\n");
            sb_appendf(&gen.sb, "    movq %%rbp, %%rsp\n");
            sb_appendf(&gen.sb, "    popq %%rbx\n");
            sb_appendf(&gen.sb, "    popq %%rbp\n");

            Function func = gen.func;
            sb_appendf(&gen.sb, "    ret $%zu\n", func.param_types.count*8);

            gen.allocated = 0;
            break;
        }
        case OP_CALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            Function func = ((Symbol *)entry->val)->as.func;

            if (func.module_name.str == NULL)
                sb_appendf(&gen.sb, "    call \"%.*s\"\n", func.extern_name.len, func.extern_name.str);
            else
                sb_appendf(&gen.sb, "    call \"%.*s::%.*s\"\n", func.module_name.len, func.module_name.str, func.extern_name.len, func.extern_name.str);

            for (int64_t i = func.return_types.count-1; i >= 0; i--) {
                sb_appendf(&gen.sb, "    pushq %zu(%%rax)\n", i*8);
                if (IS_ADVANCED(func.return_types.items[i]))
                    duplicate_struct(&gen, func.return_types.items[i].advanced->structure);
            }
            break;
        }
        case OP_CCALL: {
            Hash_Entry *entry = (Hash_Entry *)op->operand;
            generate_ccall(&gen, entry);
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
            Struct structure = op->types[0].advanced->structure;

            sb_appendf(&gen.sb, "    leaq %d(%%rbp), %%rdi\n", gen.allocated - gen.func.max_allocated);
            gen.allocated += structure.size;

            sb_appendf(&gen.sb, "    addq $%lld, %%rsp\n", structure.fields.count*8);
            for (size_t i = 0; i < structure.fields.count; i++) {
                Field *field = &structure.fields.items[i];
                if (IS_ADVANCED(field->type)) {
                    sb_appendf(&gen.sb, "    movq %d(%%rsp), %%rsi\n", -i*8 - 8);
                    move_struct(&gen, field->type.advanced->structure, field->offset);
                }
                else {
                    sb_appendf(&gen.sb, "    mov%c %lld(%%rsp), %s\n", size_sufs[type_size(field->type)], -i*8 - 8, rax[type_size(field->type)]);
                    sb_appendf(&gen.sb, "    mov%c %s, %lld(%%rdi)\n", size_sufs[type_size(field->type)], rax[type_size(field->type)], field->offset);
                }
            }
            sb_appendf(&gen.sb, "    pushq %%rdi\n");
            break;
        }
        case OP_ACCESS: {
            Field *field = (Field *)op->operand;

            sb_appendf(&gen.sb, "    movq (%%rsp), %%rsi\n");
            sb_appendf(&gen.sb, "    xorq %%rax, %%rax\n");

            if (IS_ADVANCED(field->type))
                sb_appendf(&gen.sb, "    leaq %d(%%rsi), %%rax\n", field->offset);
            else
                sb_appendf(&gen.sb, "    mov%c %d(%%rsi), %s\n", size_sufs[type_size(field->type)], field->offset, rax[type_size(field->type)]);

            sb_appendf(&gen.sb, "    pushq %%rax\n");

            if (IS_ADVANCED(field->type))
                duplicate_struct(&gen, field->type.advanced->structure);
            break;
        }
        case OP_ACCESS_DROP: {
            Field *field = (Field *)op->operand;

            sb_appendf(&gen.sb, "    popq %%rsi\n");
            sb_appendf(&gen.sb, "    xorq %%rax, %%rax\n");

            if (IS_ADVANCED(field->type))
                sb_appendf(&gen.sb, "    leaq %d(%%rsi), %%rax\n", field->offset);
            else
                sb_appendf(&gen.sb, "    mov%c %d(%%rsi), %s\n", size_sufs[type_size(field->type)], field->offset, rax[type_size(field->type)]);

            sb_appendf(&gen.sb, "    pushq %%rax\n");

            if (IS_ADVANCED(field->type))
                duplicate_struct(&gen, field->type.advanced->structure);
            break;
        }
        case OP_STORE: {
            Field *field = (Field *)op->operand;

            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rdi\n");

            if (IS_ADVANCED(field->type)) {
                sb_appendf(&gen.sb, "    movq %%rax, %%rsi\n");
                move_struct(&gen, field->type.advanced->structure, field->offset);
            }
            else {
                sb_appendf(&gen.sb, "    mov%c %s, %d(%%rdi)\n", size_sufs[type_size(field->type)], rax[type_size(field->type)], field->offset);
            }
            break;
        }
        case OP_PTR_STORE:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rdi\n");

            if (IS_ADVANCED(op->types[0])) {
                sb_appendf(&gen.sb, "    movq %%rax, %%rsi\n");
                move_struct(&gen, op->types[0].advanced->structure, 0);
            }
            else {
                sb_appendf(&gen.sb, "    mov%c %s, (%%rdi)\n", size_sufs[type_size(op->types[0])], rax[type_size(op->types[0])]);
            }
            break;
        case OP_PTR_ACCESS:
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rsi\n");

            if (IS_ADVANCED(op->types[0])) {
                sb_appendf(&gen.sb, "    pushq %%rsi\n");
                duplicate_struct(&gen, op->types[0].advanced->structure);
            }
            else {
                sb_appendf(&gen.sb, "    pushq (%%rsi)\n");
            }
            break;
        case OP_PTR_ACCESS_DROP:
            if (IS_ADVANCED(op->types[0])) {
                duplicate_struct(&gen, op->types[0].advanced->structure);
            }
            else {
                sb_appendf(&gen.sb, "    popq %%rsi\n");
                sb_appendf(&gen.sb, "    pushq (%%rsi)\n");
            }
            break;
        case OP_INDEX:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rsi\n");

            if (type_size(op->types[0]) > 1)
                sb_appendf(&gen.sb, "    imulq $%d, %%rax, %%rax\n", type_size(op->types[0]));
            sb_appendf(&gen.sb, "    add %%rax, %%rsi\n");

            if (IS_ADVANCED(op->types[0])) {
                sb_appendf(&gen.sb, "    pushq %%rsi\n");
                duplicate_struct(&gen, op->types[0].advanced->structure);
            }
            else {
                sb_appendf(&gen.sb, "    pushq (%%rsi)\n");
            }
            break;
        case OP_INDEX_STORE:
            sb_appendf(&gen.sb, "    popq %%rax\n");
            sb_appendf(&gen.sb, "    popq %%rdx\n");
            sb_appendf(&gen.sb, "    movq (%%rsp), %%rdi\n");

            if (type_size(op->types[0]) > 1)
                sb_appendf(&gen.sb, "    imulq $%d, %%rax, %%rax\n", type_size(op->types[0]));
            sb_appendf(&gen.sb, "    add %%rax, %%rdi\n");

            if (IS_ADVANCED(op->types[0])) {
                sb_appendf(&gen.sb, "    movq %%rdx, %%rsi\n");
                move_struct(&gen, op->types[0].advanced->structure, 0);
            }
            else {
                sb_appendf(&gen.sb, "    mov%c %s, (%%rdi)\n", size_sufs[type_size(op->types[0])], rdx[type_size(op->types[0])]);
            }
            break;
        case OP_ALLOC: {
            int size = type_size(op->types[0]);

            sb_appendf(&gen.sb, "    leaq %d(%%rbp), %%rdi\n", gen.allocated - gen.func.max_allocated);
            sb_appendf(&gen.sb, "    xorq %%rax, %%rax\n");
            for (int i = 0; i < size - (size & 7); i += 8)
                sb_appendf(&gen.sb, "    movq %%rax, %d(%%rdi)\n", i);

            if ((size & 7) == 0) break;

            uint64_t c = 8;
            while ((size & c) == 0)
                c >>= 1;

            int piece_size = size & c;
            for (int i = size - (size & 7); i < size; i += piece_size) {
                if (i + piece_size > size)
                    i = size - piece_size;
                sb_appendf(&gen.sb, "    mov%c %s, %d(%%rdi)\n", size_sufs[piece_size], rax[piece_size], i);
            }
            sb_appendf(&gen.sb, "    pushq %%rdi\n");

            gen.allocated += size;
            break;
        }
        case OP_PUSH_GLOBAL: {
            Global *global = (Global *)op->operand;
            sb_appendf(&gen.sb, "    pushq $\"%.*s::%.*s\"\n", global->module_name.len, global->module_name.str, global->name.len, global->name.str);
            break;
        }
        case OP_GLOBAL: {
            Global *global = (Global *)op->operand;
            DA_APPEND(&gen.globals, *global);
            break;
        }
        case OP_NOP: break;
        default:
            GEN_EPRINTF(op, LEVEL_ERR, "Unimplemented instruction: %s\n", opcode_spelling(op->opcode));
            gen.had_error = 1;
            break;
        }
        gen.pos++;
    }

    if (gen.strs.count > 0)
        sb_appendf(&gen.sb, ".section .rodata\n");

    for (size_t i = 0; i < gen.strs.count; i++) {
        sb_appendf(&gen.sb, ".str%zu: .asciz \"", i);
        for (size_t j = 0; gen.strs.items[i][j] != '\0'; j++)
            sb_appendf(&gen.sb, "\\%o", gen.strs.items[i][j]);
        sb_appendf(&gen.sb, "\"\n");
    }

    if (gen.globals.count > 0)
        sb_appendf(&gen.sb, ".section .bss\n");

    for (size_t i = 0; i < gen.globals.count; i++) {
        Global global = gen.globals.items[i];
        sb_appendf(&gen.sb, "\"%.*s::%.*s\": .zero %d\n", global.module_name.len, global.module_name.str,
                   global.name.len, global.name.str, type_size(global.type));
    }

    if (gen.had_error) {
        free(gen.sb.items);
        return NULL;
    }

    size_t len = strlen(output_file) + 1;
    char *output_asm = malloc(len);
    snprintf(output_asm, len, "%.*ss", len - 2, output_file);

    FILE *out = fopen(output_asm, "wb");
    if (!out) {
        fprintf(stderr, "\x1b[31mFailed to create output file\x1b[0m\n");
        free(gen.sb.items);
        return NULL;
    }

    fwrite(gen.sb.items, sizeof(char), gen.sb.count, out);
    if (ferror(out)) {
        fprintf(stderr, "\x1b[31mFailed to write to output file\x1b[0m\n");
        free(gen.sb.items);
        return NULL;
    }

    fclose(out);

    free(gen.sb.items);
    return output_asm;
}

