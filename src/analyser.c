#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analyser.h"
#include "compiler.h"
#include "utils.h"

#define EPRINTF_AT_OP(op, level, ...) eprintf((op)->file_path, (op)->line, (op)->pos, level, __VA_ARGS__);

char *type_spelling(Type type) {
    switch (type) {
    case TYPE_VOID:    return "void";
    case TYPE_I8:      return "i8";
    case TYPE_CHAR:    return "char";
    case TYPE_U8:      return "u8";
    case TYPE_I16:     return "i16";
    case TYPE_U16:     return "u16";
    case TYPE_I32:     return "i32";
    case TYPE_U32:     return "u32";
    case TYPE_I64:     return "i64";
    case TYPE_U64:     return "u64";
    case TYPE_F32:     return "f32";
    case TYPE_F64:     return "f64";
    case TYPE_STR:     return "str";
    case TYPE_INTEGER: return "int_lit";
    case TYPE_REAL:    return "real_lit";
    default:           return "unknown";
    }
}

inline void init_analyser(Analyser *analyser, Ops *ops) {
    *analyser = (Analyser){0};
    analyser->ops = ops;
}

static inline Type pop(Analyser *analyser) {
    if (analyser->stack.count == analyser->block_start) {
        DA_APPEND(&analyser->expected_types, analyser->stack.items[analyser->stack.count-1]);
        analyser->block_start--;
    }
    return analyser->stack.items[--analyser->stack.count];
}

inline int check_operand_count(Analyser *analyser, size_t expected) {
    Op *op = &analyser->ops->items[analyser->pos];
    if (analyser->stack.count < expected) {
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Not enough items on the stack for %s\n", opcode_spelling(op->opcode));
        return 0;
    }
    return 1;
}

void check_expected_types(Analyser *analyser) {
    Op *op = &analyser->ops->items[analyser->pos];
    if (analyser->stack.count - analyser->block_start != analyser->expected_types.count - analyser->expected_types_start)
        goto ERROR;

    for (size_t i = analyser->expected_types_start; i < analyser->expected_types.count; i++) {
        if ((analyser->expected_types.items[i] & analyser->stack.items[analyser->block_start + i - analyser->expected_types_start]) == 0) {
ERROR:
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Blocks cannot change the state of the stack, expected types: [ ");

            for (size_t j = analyser->expected_types_start; j < analyser->expected_types.count; j++)
                fprintf(stderr, "%s ", type_spelling(analyser->expected_types.items[j]));

            fprintf(stderr, "], actual types: [ ");
            for (size_t j = analyser->block_start; j < analyser->stack.count; j++)
                fprintf(stderr, "%s ", type_spelling(analyser->stack.items[j]));

            fprintf(stderr, "]\n");

            analyser->stack.count = analyser->block_start + analyser->expected_types.count - analyser->expected_types_start;
            DA_EXPAND(&analyser->stack, analyser->stack.count);
            memcpy(analyser->stack.items + analyser->block_start, analyser->expected_types.items + analyser->expected_types_start,
                   (analyser->expected_types.count - analyser->expected_types_start) * sizeof(Type));
            break;
        }
    }
}

void type_check_op(Analyser *analyser);

void type_check_block(Analyser *analyser) {
    int block_start = analyser->block_start;
    int expected_types_start = analyser->expected_types_start;
    analyser->block_start = analyser->stack.count;
    analyser->expected_types_start = analyser->expected_types.count;

    if (analyser->ops->items[analyser->pos].operand < UINT64_MAX)
        analyser->ops->items[analyser->pos].opcode = OP_LABEL;
    else
        analyser->ops->items[analyser->pos].opcode = OP_NOP;

    analyser->pos++;
    while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->pos < analyser->ops->count)
        type_check_op(analyser);

    if (analyser->pos == analyser->ops->count) {
        EPRINTF_AT_OP(&analyser->ops->items[analyser->pos-1], LEVEL_ERR, "Analyser did not reach end of block. This is a bug.\n");
        exit(1);
    }

    analyser->ops->items[analyser->pos].opcode = OP_LABEL;

    check_expected_types(analyser);
    analyser->block_start = block_start;
    analyser->expected_types.count = analyser->expected_types_start;
    analyser->expected_types_start = expected_types_start;
}

void type_check_if(Analyser *analyser) {
    int block_start = analyser->block_start;
    int expected_types_start = analyser->expected_types_start;
    analyser->block_start = analyser->stack.count;
    analyser->expected_types_start = analyser->expected_types.count;

    analyser->ops->items[analyser->pos].opcode = OP_NOP;

    Types copy = {0};
    copy.count = analyser->stack.count;
    DA_EXPAND(&copy, copy.count);
    memcpy(copy.items, analyser->stack.items, analyser->stack.count * sizeof(Type));

    analyser->pos++;
    while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->ops->items[analyser->pos].opcode != OP_ELSE
        && analyser->pos < analyser->ops->count)
        type_check_op(analyser);

    if (analyser->pos == analyser->ops->count) {
        EPRINTF_AT_OP(&analyser->ops->items[analyser->pos-1], LEVEL_ERR, "Analyser did not reach end of block. This is a bug.\n");
        exit(1);
    }

    if (analyser->ops->items[analyser->pos].opcode == OP_ELSE) {
        analyser->ops->items[analyser->pos].opcode = OP_LABEL;

        DA_EXPAND(&analyser->expected_types, analyser->expected_types_start + analyser->stack.count - analyser->block_start);
        analyser->expected_types.count = analyser->expected_types_start + analyser->stack.count - analyser->block_start;
        memcpy(analyser->expected_types.items + analyser->expected_types_start, analyser->stack.items + analyser->block_start,
               (analyser->stack.count - analyser->block_start) * sizeof(Type));

        analyser->stack.count = copy.count;
        memcpy(analyser->stack.items, copy.items, copy.count * sizeof(Type));

        while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->pos < analyser->ops->count)
            type_check_op(analyser);

        if (analyser->pos == analyser->ops->count) {
            EPRINTF_AT_OP(&analyser->ops->items[analyser->pos-1], LEVEL_ERR, "Analyser did not reach end of block. This is a bug.\n");
            exit(1);
        }
    }

    analyser->ops->items[analyser->pos].opcode = OP_LABEL;

    check_expected_types(analyser);
    analyser->block_start = block_start;
    analyser->expected_types.count = analyser->expected_types_start;
    analyser->expected_types_start = expected_types_start;

    free(copy.items);
}

void type_check_op(Analyser *analyser) {
    Op *op = &analyser->ops->items[analyser->pos];
    switch (op->opcode) {
    case OP_PUSH:
        DA_APPEND(&analyser->stack, op->types[0]);
        break;
    case OP_SUB:
    case OP_ADD: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);

        if ((a & TYPE_INTEGER && b & TYPE_INTEGER) || (a & TYPE_REAL && b & TYPE_REAL)
            || (a & (TYPE_INTEGER | TYPE_STR) && b & (TYPE_INTEGER | TYPE_STR) && (op->opcode != OP_ADD || a != b))) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
            op->types[1] = b == TYPE_INTEGER ? TYPE_I64 : b == TYPE_REAL ? TYPE_F64 : b;
            DA_APPEND(&analyser->stack, op->types[0] > op->types[1] ? op->types[0] : op->types[1]);
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand types for %s: %s %s\n",
                             opcode_spelling(op->opcode), type_spelling(b), type_spelling(a));
        }
        break;
    }
    case OP_DIV:
    case OP_MUL: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);

        if ((a & TYPE_INTEGER && b & TYPE_INTEGER) || (a & TYPE_REAL && b & TYPE_REAL)) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
            op->types[1] = b == TYPE_INTEGER ? TYPE_I64 : b == TYPE_REAL ? TYPE_F64 : b;
            DA_APPEND(&analyser->stack, op->types[0] > op->types[1] ? op->types[0] : op->types[1]);
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand types for %s: %s %s\n",
                             opcode_spelling(op->opcode), type_spelling(b), type_spelling(a));
        }
        break;
    }
    case OP_MOD:
    case OP_ROR:
    case OP_ROL:
    case OP_SHR:
    case OP_SHL:
    case OP_XOR:
    case OP_OR:
    case OP_AND: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);

        if (a & TYPE_INTEGER && b & TYPE_INTEGER) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a;
            op->types[1] = b == TYPE_INTEGER ? TYPE_I64 : b;
            DA_APPEND(&analyser->stack, op->types[0] > op->types[1] ? op->types[0] : op->types[1]);
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand types for %s: %s %s\n",
                             opcode_spelling(op->opcode), type_spelling(b), type_spelling(a));
        }
        break;
    }
    case OP_NOT: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = analyser->stack.items[analyser->stack.count-1];

        if (a & TYPE_INTEGER) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for NOT: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_DUP: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = analyser->stack.items[analyser->stack.count-1];
        DA_APPEND(&analyser->stack, a);
        break;
    }
    case OP_OVER: {
        if (!check_operand_count(analyser, 2)) break;
        Type a = analyser->stack.items[analyser->stack.count-2];
        DA_APPEND(&analyser->stack, a);
        break;
    }
    case OP_DUP2: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = analyser->stack.items[analyser->stack.count-2];
        DA_APPEND(&analyser->stack, a);

        a = analyser->stack.items[analyser->stack.count-2];
        DA_APPEND(&analyser->stack, a);
        break;
    }
    case OP_DROP:
        if (!check_operand_count(analyser, 1)) break;
        pop(analyser);
        break;
    case OP_SWAP: {
        if (!check_operand_count(analyser, 2)) break;
        Type a = pop(analyser);
        Type b = pop(analyser);

        DA_APPEND(&analyser->stack, a);
        DA_APPEND(&analyser->stack, b);
        break;
    }
    case OP_OVER2: {
        if (!check_operand_count(analyser, 4)) break;

        Type a = analyser->stack.items[analyser->stack.count-4];
        DA_APPEND(&analyser->stack, a);

        a = analyser->stack.items[analyser->stack.count-4];
        DA_APPEND(&analyser->stack, a);
        break;
    }
    case OP_SWAP2: {
        if (!check_operand_count(analyser, 4)) break;

        Type a0 = pop(analyser);
        Type b0 = pop(analyser);
        Type a1 = pop(analyser);
        Type b1 = pop(analyser);

        DA_APPEND(&analyser->stack, b0);
        DA_APPEND(&analyser->stack, a0);
        DA_APPEND(&analyser->stack, b1);
        DA_APPEND(&analyser->stack, a1);
        break;
    }
    case OP_NEG: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = analyser->stack.items[analyser->stack.count-1];

        if (a & TYPE_NUMBER) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for NEG: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_GTEQ:
    case OP_GT:
    case OP_LTEQ:
    case OP_LT:
    case OP_EQ: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);

        if ((a & TYPE_INTEGER && b & TYPE_INTEGER) || (a & TYPE_REAL && b & TYPE_REAL)) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
            op->types[1] = b == TYPE_INTEGER ? TYPE_I64 : b == TYPE_REAL ? TYPE_F64 : b;
            DA_APPEND(&analyser->stack, TYPE_U8);
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand types for %s: %s %s\n",
                             opcode_spelling(op->opcode), type_spelling(b), type_spelling(a));
        }
        break;
    }
    case OP_JMPF: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = pop(analyser);

        if (a & TYPE_INTEGER) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for JMPF: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_JMP:
        if (op[1].opcode != OP_ELSE && op[1].opcode != OP_END)
            check_expected_types(analyser);
        break;
    case OP_FUNC: {
        Hash_Entry *func_entry = (Hash_Entry *)op->operand;
        analyser->func = ((Symbol *)func_entry->val)->as.func;
        for (size_t i = 0; i < analyser->func.param_types.count; i++)
            DA_APPEND(&analyser->stack, analyser->func.param_types.items[i]);
        break;
    }
    case OP_RET: {
        Function func = analyser->func;
        if (!check_operand_count(analyser, func.return_types.count)) break;

        for (size_t i = 0; i < func.return_types.count; i++) {
            Type a = analyser->stack.items[analyser->stack.count-func.return_types.count+i];

            if ((a & func.return_types.items[i]) == 0) {
                analyser->had_error = 1;
                EPRINTF_AT_OP(op, LEVEL_ERR, "Return types don't match expected return types, expected types: [ ");

                for (size_t j = 0; j < func.return_types.count; j++)
                    fprintf(stderr, "%s ", type_spelling(func.return_types.items[j]));

                fprintf(stderr, "], actual types: [ ");
                for (size_t j = analyser->stack.count - func.return_types.count; j < analyser->stack.count; j++)
                    fprintf(stderr, "%s ", type_spelling(analyser->stack.items[j]));

                fprintf(stderr, "]\n");
                break;
            }
        }

        analyser->stack.count = 0;
        break;
    }
    case OP_LNOT: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = pop(analyser);

        if (a & TYPE_NUMBER) {
            op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
            DA_APPEND(&analyser->stack, TYPE_U8);
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for LNOT: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_CCALL:
    case OP_CALL: {
        Function func = ((Symbol *)((Hash_Entry *)op->operand)->val)->as.func;
        if (!check_operand_count(analyser, func.param_types.count)) break;

        for (size_t i = 0; i < func.param_types.count; i++) {
            Type arg = analyser->stack.items[analyser->stack.count-func.param_types.count+i];

            if ((arg & func.param_types.items[i]) == 0) {
                analyser->had_error = 1;
                EPRINTF_AT_OP(op, LEVEL_ERR, "Argument types don't match function parameters, expected types: [ ");

                for (size_t j = 0; j < func.param_types.count; j++)
                    fprintf(stderr, "%s ", type_spelling(func.param_types.items[j]));

                fprintf(stderr, "], actual types: [ ");
                for (size_t j = analyser->stack.count-i-1; j < analyser->stack.count; j++)
                    fprintf(stderr, "%s ", type_spelling(analyser->stack.items[j]));

                fprintf(stderr, "]\n");
                break;
            }
        }
        analyser->stack.count -= func.param_types.count;

        for (size_t i = 0; i < func.return_types.count; i++)
            DA_APPEND(&analyser->stack, func.return_types.items[i]);
        break;
    }
    case OP_STR:
        DA_APPEND(&analyser->stack, TYPE_STR);
        break;
    case OP_ROT: {
        if (!check_operand_count(analyser, 3)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);
        Type c = pop(analyser);

        op->types[0] = a;
        op->types[1] = b;
        op->types[2] = c;

        DA_APPEND(&analyser->stack, b);
        DA_APPEND(&analyser->stack, a);
        DA_APPEND(&analyser->stack, c);
        break;
    }
    case OP_CONVERT: {
        Type a = analyser->stack.items[analyser->stack.count-1];
        op->types[0] = a == TYPE_INTEGER ? TYPE_I64 : a == TYPE_REAL ? TYPE_F64 : a;
        analyser->stack.items[analyser->stack.count-1] = op->types[1];
        break;
    }
    case OP_LABEL: break;
    case OP_START:
        type_check_block(analyser);
        break;
    case OP_IF:
        type_check_if(analyser);
        break;
    default:
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Unimplemented instruction in type checking: %s\n", opcode_spelling(op->opcode));
        break;
    }

    analyser->pos++;
}

int type_check(Ops *ops) {
    Analyser analyser;
    init_analyser(&analyser, ops);

    while (analyser.pos < ops->count)
        type_check_op(&analyser);

    return analyser.had_error;
}

