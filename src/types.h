#ifndef ARROW_TYPES_H
#define ARROW_TYPES_H

#include "utils.h"

#define BASIC_TYPE(type) (Type){ .kind = KIND_BASIC, .as.basic = type }
#define ADVANCED_TYPE(type) (Type){ .kind = KIND_ADVANCED, .as.advanced = type }

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
    KIND_BASIC, KIND_ADVANCED,
} Type_Kind;

typedef enum {
    KIND_STRUCT,
} Advanced_Type_Kind;

typedef enum {
    STATUS_UNRESOLVED, STATUS_RESOLVING,
    STATUS_RESOLVED,
} Resolve_Status;

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
    Advanced_Type_Kind kind;
    union {
        Struct structure;
    } as;
    Resolve_Status resolve_status;
    Loc loc;
} Advanced_Type;

typedef struct {
    Type_Kind kind;
    union {
        Basic_Type basic;
        Advanced_Type *advanced;
    } as;
} Type;

typedef struct {
    size_t count;
    size_t capacity;
    Type *items;
} Types;

typedef struct {
    size_t count;
    size_t capacity;
    Advanced_Type *items;
} Advanced_Types;

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

char *basic_type_spelling(Basic_Type type);
char *type_spelling(Type type);

#endif // ARROW_TYPES_H
