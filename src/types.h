#ifndef ARROW_TYPES_H
#define ARROW_TYPES_H

#include "utils.h"

#define BASIC_TYPE(type) (Type){ .kind = KIND_BASIC, .as.basic = type }

typedef enum {
    TYPE_VOID = 0x0,
    TYPE_I8   = 0x1,   TYPE_CHAR = 0x2,
    TYPE_U8   = 0x4,   TYPE_I16  = 0x8,
    TYPE_U16  = 0x10,  TYPE_I32  = 0x20,
    TYPE_U32  = 0x40,  TYPE_I64  = 0x80,
    TYPE_U64  = 0x100, TYPE_F32  = 0x200,
    TYPE_F64  = 0x400, TYPE_STR  = 0x800,

    TYPE_INTEGER = TYPE_I8  | TYPE_CHAR | TYPE_U8  | TYPE_I16 |
                   TYPE_U16 | TYPE_I32  | TYPE_U32 | TYPE_I64 | TYPE_U64,
    TYPE_REAL    = TYPE_F32 | TYPE_F64,
    TYPE_NUMBER  = TYPE_INTEGER | TYPE_REAL,
} Basic_Type;

typedef enum {
    KIND_BASIC, KIND_STRUCT,
} Type_Kind;

typedef struct Field Field;

typedef struct {
    size_t count;
    size_t capacity;
    Field *items;
} Fields;

typedef struct {
    String_View name;
    int size;
    int alignment;
    Fields fields;
} Struct;

typedef struct {
    Type_Kind kind;
    union {
        Basic_Type basic;
        Struct structure;
    } as;
} Type;

typedef struct {
    size_t count;
    size_t capacity;
    Type *items;
} Types;

struct Field {
    String_View name;
    Type type;
    size_t offset;
};

int type_size(Type type);
int type_alignment(Type type);
int typeid(Type type);

int types_compatible(Type a, Type b);
int types_equal(Type a, Type b);

int struct_size(Struct structure);
Field *get_first_leaf_field(Struct structure);
Field *get_last_leaf_field(Struct structure);

#endif // ARROW_TYPES_H
