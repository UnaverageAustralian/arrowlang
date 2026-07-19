#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analyser.h"
#include "compiler.h"
#include "types.h"
#include "utils.h"

#define EPRINTF_AT_OP(op, level, ...) eprintf((op)->file_path, (op)->line, (op)->pos, level, __VA_ARGS__);

inline void init_analyser(Analyser *analyser, Ops *ops) {
    *analyser = (Analyser){0};
    analyser->ops = ops;
}

static inline Type peek(Analyser *analyser, int count) {
    return analyser->stack.items[analyser->stack.count-count];
}

static inline Type pop(Analyser *analyser) {
    if (analyser->stack.count == analyser->block_start) {
        DA_APPEND(&analyser->expected_types, peek(analyser, 1));
        analyser->block_start--;
    }
    return analyser->stack.items[--analyser->stack.count];
}

inline int check_operand_count(Analyser *analyser, size_t expected) {
    Op *op = &analyser->ops->items[analyser->pos];
    if (analyser->stack.count < expected) {
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Not enough items on the stack for %s, expected at least %d elements\n",
                      opcode_spelling(op->opcode), expected);
        return 0;
    }
    return 1;
}

inline void make_conversion_op(Analyser *analyser, Type greater, Type lesser, int operand) {
    Op cur = analyser->ops->items[analyser->pos];
    Op op = {
        .opcode = OP_CONVERT,
        .file_path = cur.file_path,
        .line = cur.line,
        .pos = cur.pos,
        .operand = operand,
        .types = { lesser, greater },
    };
    DA_APPEND(&analyser->dst, op);
}

void check_expected_types(Analyser *analyser) {
    Op *op = &analyser->ops->items[analyser->pos];

    int expected_count = analyser->in_block ? analyser->expected_types.count - analyser->expected_types_start : analyser->func->return_types.count;
    int actual_count = analyser->stack.count - analyser->block_start;

    if (actual_count < expected_count && !analyser->in_block) {
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Not enough items on the stack for returning, expected at least %d elements\n", expected_count);
        return;
    }

    int had_error = 0;
    if (actual_count != expected_count && analyser->in_block)
        had_error = 1;

    for (size_t i = analyser->expected_types_start; i < analyser->expected_types.count; i++) {
        if (had_error) break;
        if (!types_compatible(analyser->expected_types.items[i], analyser->stack.items[analyser->stack.count-1 - i + analyser->expected_types_start])) {
            had_error = 1;
            break;
        }
    }

    if (had_error) {
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Blocks cannot change the state of the stack, expected types: [ ");

        for (size_t j = analyser->expected_types_start; j < analyser->expected_types.count; j++)
            fprintf(stderr, "%s ", type_spelling(analyser->expected_types.items[j]));

        fprintf(stderr, "], actual types: [ ");
        for (size_t j = analyser->block_start; j < analyser->stack.count; j++)
            fprintf(stderr, "%s ", type_spelling(analyser->stack.items[j]));

        fprintf(stderr, "]\n");
    }
}

void type_check_op(Analyser *analyser);

void type_check_block(Analyser *analyser) {
    int in_block = analyser->in_block;
    analyser->in_block = 1;

    int block_start = analyser->block_start;
    int expected_types_start = analyser->expected_types_start;
    analyser->block_start = analyser->stack.count;
    analyser->expected_types_start = analyser->expected_types.count;

    if (analyser->ops->items[analyser->pos].operand < UINT64_MAX) {
        analyser->ops->items[analyser->pos].opcode = OP_LABEL;
        DA_APPEND(&analyser->dst, analyser->ops->items[analyser->pos]);
    }

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

    analyser->in_block = in_block;
}

void type_check_if(Analyser *analyser) {
    int in_block = analyser->in_block;
    analyser->in_block = 1;

    analyser->ops->items[analyser->pos].opcode = OP_JMPF;
    type_check_op(analyser);

    int block_start = analyser->block_start;
    int expected_types_start = analyser->expected_types_start;
    analyser->block_start = analyser->stack.count;
    analyser->expected_types_start = analyser->expected_types.count;

    Types copy = {0};
    copy.count = analyser->stack.count;
    DA_EXPAND(&copy, copy.count);
    memcpy(copy.items, analyser->stack.items, analyser->stack.count * sizeof(Type));

    while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->ops->items[analyser->pos].opcode != OP_ELSE && 
           analyser->ops->items[analyser->pos].opcode != OP_ELSEIF && analyser->pos < analyser->ops->count)
        type_check_op(analyser);

    while (analyser->ops->items[analyser->pos].opcode == OP_ELSEIF && analyser->pos < analyser->ops->count) {
        analyser->ops->items[analyser->pos].opcode = OP_LABEL;

        DA_EXPAND(&analyser->expected_types, analyser->expected_types_start + analyser->stack.count - analyser->block_start);
        analyser->expected_types.count = analyser->expected_types_start + analyser->stack.count - analyser->block_start;
        memcpy(analyser->expected_types.items + analyser->expected_types_start, analyser->stack.items + analyser->block_start,
               (analyser->stack.count - analyser->block_start) * sizeof(Type));

        analyser->stack.count = copy.count;
        memcpy(analyser->stack.items, copy.items, copy.count * sizeof(Type));

        Types expected_types = analyser->expected_types;
        analyser->expected_types = copy;

        while (analyser->ops->items[analyser->pos-1].opcode != OP_JMPF && analyser->pos < analyser->ops->count)
            type_check_op(analyser);
        check_expected_types(analyser);

        analyser->expected_types = expected_types;
        while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->ops->items[analyser->pos].opcode != OP_ELSE && 
               analyser->ops->items[analyser->pos].opcode != OP_ELSEIF && analyser->pos < analyser->ops->count)
            type_check_op(analyser);

        check_expected_types(analyser);
    }

    if (analyser->ops->items[analyser->pos].opcode == OP_END) {
        analyser->expected_types.count = analyser->expected_types_start;
    }
    else {
        analyser->ops->items[analyser->pos].opcode = OP_LABEL;

        DA_EXPAND(&analyser->expected_types, analyser->expected_types_start + analyser->stack.count - analyser->block_start);
        analyser->expected_types.count = analyser->expected_types_start + analyser->stack.count - analyser->block_start;
        memcpy(analyser->expected_types.items + analyser->expected_types_start, analyser->stack.items + analyser->block_start,
               (analyser->stack.count - analyser->block_start) * sizeof(Type));

        analyser->stack.count = copy.count;
        memcpy(analyser->stack.items, copy.items, copy.count * sizeof(Type));

        while (analyser->ops->items[analyser->pos].opcode != OP_END && analyser->pos < analyser->ops->count)
            type_check_op(analyser);
        check_expected_types(analyser);
    }

    if (analyser->pos == analyser->ops->count) {
        EPRINTF_AT_OP(&analyser->ops->items[analyser->pos-1], LEVEL_ERR, "Analyser did not reach end of block. This is a bug.\n");
        exit(1);
    }

    analyser->ops->items[analyser->pos].opcode = OP_LABEL;

    check_expected_types(analyser);
    analyser->block_start = block_start;
    analyser->expected_types.count = analyser->expected_types_start;
    analyser->expected_types_start = expected_types_start;

    free(copy.items);
    analyser->in_block = in_block;
}

void expected_types_error(Analyser *analyser, char *msg, Types expected) {
    analyser->had_error = 1;
    EPRINTF_AT_OP(&analyser->ops->items[analyser->ops->count-1], LEVEL_ERR, "%s, expected types: [ ", msg);

    for (size_t i = 0; i < expected.count; i++)
        fprintf(stderr, "%s ", type_spelling(expected.items[i]));

    fprintf(stderr, "], actual types: [ ");
    for (size_t i = analyser->stack.count - expected.count; i < analyser->stack.count; i++)
        fprintf(stderr, "%s ", type_spelling(analyser->stack.items[i]));

    fprintf(stderr, "]\n");
}

Field *find_field(Struct *structure, String_View *sv) {
    for (size_t i = 0; i < structure->fields.count; i++) {
        if (sv->len != structure->fields.items[i].name.len) continue;
        if (strncmp(sv->str, structure->fields.items[i].name.str, sv->len) == 0)
            return &structure->fields.items[i];
    }
    return NULL;
}

int binop_operands_valid(Op *op, Type a, Type b) {
    int valid = a.kind == KIND_BASIC && b.kind == KIND_BASIC;
    switch (op->operand) {
    case OP_ADD:
    case OP_SUB:
        valid |= (a.as.basic & TYPE_INTEGER && b.as.basic & TYPE_INTEGER) || (a.as.basic & TYPE_REAL && b.as.basic & TYPE_REAL) ||
                 (a.as.basic & (TYPE_INTEGER | TYPE_STR) && b.as.basic & (TYPE_INTEGER | TYPE_STR) && (op->opcode != OP_ADD || a.as.basic != b.as.basic));
        break;
    case OP_MUL:
    case OP_DIV:
    case OP_NEQ:
    case OP_GTEQ:
    case OP_GT:
    case OP_LTEQ:
    case OP_LT:
    case OP_EQ:
        valid |= (a.as.basic & TYPE_INTEGER && b.as.basic & TYPE_INTEGER) || (a.as.basic & TYPE_REAL && b.as.basic & TYPE_REAL);
        break;
    case OP_MOD:
    case OP_ROR:
    case OP_ROL:
    case OP_SHR:
    case OP_SHL:
    case OP_XOR:
    case OP_OR:
    case OP_AND:
        valid |= a.as.basic & TYPE_INTEGER && b.as.basic & TYPE_INTEGER;
        break;
    }
    return valid;
}

static inline void allocate(Analyser *analyser, Type a) {
    analyser->allocated += a.as.advanced->as.structure.size;
    if (analyser->allocated > analyser->max_allocated)
        analyser->max_allocated = analyser->allocated;
}

void type_check_op(Analyser *analyser) {
    Op *op = &analyser->ops->items[analyser->pos];
    switch (op->opcode) {
    case OP_PUSH:
        DA_APPEND(&analyser->stack, op->types[0]);
        break;
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
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

        int depth;
        Type lesser, greater;

        if (a.as.basic > b.as.basic) {
            lesser = b;
            greater = a;
            depth = 8;
        }
        else {
            greater = b;
            lesser = a;
            depth = 0;
        }

        if (binop_operands_valid(op, a, b)) {
            if (b.as.basic == TYPE_INTEGER || b.as.basic == TYPE_REAL) {
                a.as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
                greater = a;
                lesser.as.basic = b.as.basic == TYPE_INTEGER ? TYPE_I64 : TYPE_F64;
                depth = 8;
                b = a;
            }
            else if (a.as.basic == TYPE_INTEGER || a.as.basic == TYPE_REAL) {
                greater = b;
                lesser.as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : TYPE_F64;
                depth = 0;
                a = b;
            }

            op->types[0] = greater;
            DA_APPEND(&analyser->stack, a.as.basic > b.as.basic ? a : b);

            if (greater.as.basic != lesser.as.basic)
                make_conversion_op(analyser, greater, lesser, depth);
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
        Type a = peek(analyser, 1);

        if (a.kind == KIND_BASIC && a.as.basic & TYPE_INTEGER) {
            op->types[0].as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for NOT: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_DUP: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = peek(analyser, 1);
        DA_APPEND(&analyser->stack, a);
        op->types[0] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);
        break;
    }
    case OP_OVER: {
        if (!check_operand_count(analyser, 2)) break;
        Type a = peek(analyser, 2);
        DA_APPEND(&analyser->stack, a);
        op->types[0] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);
        break;
    }
    case OP_DUP2: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = peek(analyser, 2);
        DA_APPEND(&analyser->stack, a);
        op->types[0] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);

        a = peek(analyser, 2);
        DA_APPEND(&analyser->stack, a);
        op->types[1] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);
        break;
    }
    case OP_DROP: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = pop(analyser);
        op->types[0] = a;
        break;
    }
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

        Type a = peek(analyser, 4);
        DA_APPEND(&analyser->stack, a);
        op->types[0] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);

        a = peek(analyser, 4);
        DA_APPEND(&analyser->stack, a);
        op->types[1] = a;

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, a);
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
        Type a = peek(analyser, 1);

        if (a.kind == KIND_BASIC && a.as.basic & TYPE_NUMBER) {
            op->types[0].as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for NEG: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_NEQ:
    case OP_GTEQ:
    case OP_GT:
    case OP_LTEQ:
    case OP_LT:
    case OP_EQ: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);

        int depth;
        Type lesser, greater;

        if (a.as.basic > b.as.basic) {
            lesser = b;
            greater = a;
            depth = 8;
        }
        else {
            greater = b;
            lesser = a;
            depth = 0;
        }

        if (binop_operands_valid(op, a, b)) {
            if (b.as.basic == TYPE_INTEGER || b.as.basic == TYPE_REAL) {
                a.as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
                greater = a;
                lesser.as.basic = b.as.basic == TYPE_INTEGER ? TYPE_I64 : TYPE_F64;
                depth = 8;
                b = a;
            }
            else if (a.as.basic == TYPE_INTEGER || a.as.basic == TYPE_REAL) {
                greater = b;
                lesser.as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : TYPE_F64;
                depth = 0;
                a = b;
            }

            op->types[0] = greater;
            DA_APPEND(&analyser->stack, BASIC_TYPE(TYPE_U8));

            if (greater.as.basic != lesser.as.basic)
                make_conversion_op(analyser, greater, lesser, depth);
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

        if (a.kind == KIND_BASIC && a.as.basic & TYPE_INTEGER) {
            op->types[0].as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic;
        }
        else {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Invalid operand type for JMPF: %s\n", type_spelling(a));
        }
        break;
    }
    case OP_JMP:
        if (op[1].opcode != OP_ELSE && op[1].opcode != OP_END && op[1].opcode != OP_ELSEIF)
            check_expected_types(analyser);
        break;
    case OP_FUNC: {
        Hash_Entry *func_entry = (Hash_Entry *)op->operand;
        analyser->func = &((Symbol *)func_entry->val)->as.func;
        for (size_t i = 0; i < analyser->func->param_types.count; i++)
            DA_APPEND(&analyser->stack, analyser->func->param_types.items[i]);
        break;
    }
    case OP_RET: {
        Function func = *analyser->func;
        if (!check_operand_count(analyser, func.return_types.count)) break;

        for (size_t i = 0; i < func.return_types.count; i++) {
            Type a = peek(analyser, func.return_types.count - i);

            if (!types_compatible(a, func.return_types.items[i])) {
                expected_types_error(analyser, "Return types don't match expected return types", func.return_types);
                break;
            }
        }

        analyser->stack.count = 0;
        analyser->allocated = 0;

        analyser->func->max_allocated = analyser->max_allocated;
        break;
    }
    case OP_LNOT: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = pop(analyser);

        if (a.kind == KIND_BASIC && a.as.basic & TYPE_NUMBER) {
            op->types[0].as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
            DA_APPEND(&analyser->stack, BASIC_TYPE(TYPE_U8));
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

        int had_error = 0;
        for (size_t i = 0; i < func.param_types.count; i++) {
            Type arg = peek(analyser, func.param_types.count - i);
            Type param = func.param_types.items[i];

            if (!types_compatible(arg, param) && !had_error) {
                had_error = 1;
                continue;
            }

            if (!types_equal(arg, param)) {
                arg.as.basic = arg.as.basic == TYPE_INTEGER ? TYPE_I64 : arg.as.basic == TYPE_REAL ? TYPE_F64 : arg.as.basic;
                make_conversion_op(analyser, func.param_types.items[i], arg, (func.param_types.count-i-1)*8);
            }
        }

        if (had_error) {
            expected_types_error(analyser, "Argument types don't match function parameters", func.param_types);
            break;
        }

        analyser->stack.count -= func.param_types.count;
        for (size_t i = 0; i < func.return_types.count; i++) {
            DA_APPEND(&analyser->stack, func.return_types.items[i]);
            if (func.return_types.items[i].kind == KIND_ADVANCED && func.return_types.items[i].as.advanced->kind == KIND_STRUCT)
                allocate(analyser, func.return_types.items[i]);
        }
        break;
    }
    case OP_STR:
        DA_APPEND(&analyser->stack, BASIC_TYPE(TYPE_STR));
        break;
    case OP_ROT: {
        if (!check_operand_count(analyser, 3)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);
        Type c = pop(analyser);

        DA_APPEND(&analyser->stack, b);
        DA_APPEND(&analyser->stack, a);
        DA_APPEND(&analyser->stack, c);
        break;
    }
    case OP_ROTN: {
        if (!check_operand_count(analyser, 3)) break;

        Type a = pop(analyser);
        Type b = pop(analyser);
        Type c = pop(analyser);

        DA_APPEND(&analyser->stack, a);
        DA_APPEND(&analyser->stack, c);
        DA_APPEND(&analyser->stack, b);
        break;
    }
    case OP_CONVERT: {
        if (!check_operand_count(analyser, 1)) break;
        Type a = pop(analyser);

        if (a.kind == KIND_ADVANCED && a.as.advanced->kind == KIND_STRUCT) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Cannot convert a struct to a basic type\n");
            break;
        }

        op->types[0].as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
        DA_APPEND(&analyser->stack, op->types[1]);
        break;
    }
    case OP_INIT: {
        Fields fields = op->types[0].as.advanced->as.structure.fields;
        if (!check_operand_count(analyser, fields.count)) break;

        int had_error = 0;
        for (size_t i = 0; i < fields.count; i++) {
            Type a = peek(analyser, fields.count - i);
            Type field = fields.items[i].type;

            if (!types_compatible(a, field) && !had_error) {
                had_error = 1;
                continue;
            }

            if (!types_equal(a, field)) {
                if (a.kind == KIND_BASIC)
                    a.as.basic = a.as.basic == TYPE_INTEGER ? TYPE_I64 : a.as.basic == TYPE_REAL ? TYPE_F64 : a.as.basic;
                make_conversion_op(analyser, field, a, (fields.count-i-1)*8);
            }
        }

        if (had_error) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Operand types don't match struct fields, expected [ ");

            for (size_t i = 0; i < fields.count; i++)
                fprintf(stderr, "%s ", type_spelling(fields.items[i].type));

            fprintf(stderr, "], actual types: [ ");
            for (size_t i = analyser->stack.count - fields.count; i < analyser->stack.count; i++)
                fprintf(stderr, "%s ", type_spelling(analyser->stack.items[i]));

            fprintf(stderr, "]\n");
        }

        analyser->stack.count -= fields.count;
        DA_APPEND(&analyser->stack, op->types[0]);

        allocate(analyser, op->types[0]);
        break;
    }
    case OP_ACCESS: {
        if (!check_operand_count(analyser, 1)) break;

        Type a = peek(analyser, 1);
        if (a.kind != KIND_ADVANCED || a.as.advanced->kind != KIND_STRUCT) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Source is not a struct\n");
            break;
        }

        Struct structure = a.as.advanced->as.structure;
        String_View *sv = (String_View *)op->operand;

        Field *field = find_field(&structure, sv);
        if (!field) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "%.*s has no field %.*s\n", structure.name.len, structure.name.str, sv->len, sv->str);
            break;
        }

        op->operand = (uint64_t)field;
        DA_APPEND(&analyser->stack, field->type);

        if (field->type.kind == KIND_ADVANCED && field->type.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, field->type);
        break;
    }
    case OP_ACCESS_DROP: {
        if (!check_operand_count(analyser, 1)) break;

        Type a = pop(analyser);
        if (a.kind != KIND_ADVANCED || a.as.advanced->kind != KIND_STRUCT) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Source is not a struct\n");
            break;
        }

        Struct structure = a.as.advanced->as.structure;
        String_View *sv = (String_View *)op->operand;

        Field *field = find_field(&structure, sv);
        if (!field) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "%.*s has no field %.*s\n", structure.name.len, structure.name.str, sv->len, sv->str);
            break;
        }

        op->operand = (uint64_t)field;
        DA_APPEND(&analyser->stack, field->type);

        if (field->type.kind == KIND_ADVANCED && field->type.as.advanced->kind == KIND_STRUCT)
            allocate(analyser, field->type);
        break;
    }
    case OP_STORE: {
        if (!check_operand_count(analyser, 2)) break;

        Type a = pop(analyser);
        Type b = peek(analyser, 1);

        if (b.kind != KIND_ADVANCED || b.as.advanced->kind != KIND_STRUCT) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Destination is not a struct\n");
            break;
        }

        Struct structure = b.as.advanced->as.structure;
        String_View *sv = (String_View *)op->operand;

        Field *field = find_field(&structure, sv);
        if (!field) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "%.*s has no field %.*s\n", structure.name.len, structure.name.str, sv->len, sv->str);
            break;
        }

        if (!types_compatible(a, field->type)) {
            analyser->had_error = 1;
            EPRINTF_AT_OP(op, LEVEL_ERR, "Source type does not match destination type, expected %s, got %s\n",
                          type_spelling(field->type), type_spelling(a));
            break;
        }

        op->operand = (uint64_t)field;
        break;
    }
    case OP_LABEL: break;
    case OP_START:
        type_check_block(analyser);
        return;
    case OP_IF:
        type_check_if(analyser);
        return;
    default:
        analyser->had_error = 1;
        EPRINTF_AT_OP(op, LEVEL_ERR, "Unimplemented instruction in type checking: %s\n", opcode_spelling(op->opcode));
        analyser->pos++;
        return;
    }

    DA_APPEND(&analyser->dst, *op);
    analyser->pos++;
}

int type_check(Ops *ops) {
    Analyser analyser;
    init_analyser(&analyser, ops);

    while (analyser.pos < ops->count)
        type_check_op(&analyser);

    free(ops->items);
    *ops = analyser.dst;

    return analyser.had_error;
}

