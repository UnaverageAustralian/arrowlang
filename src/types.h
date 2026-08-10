#ifndef ARROW_TYPES_H
#define ARROW_TYPES_H

#include <stdint.h>

#include "utils.h"

#define BASIC_TYPE(type) (Type){ .kind = type }
#define PTR_TYPE(type) (Type){ .kind = TYPE_PTR, .ptr_depth = 1, .deref_kind = type }
#define ADVANCED_TYPE(type_kind, type) (Type){ .kind = type_kind, .advanced = type }

#define IS_INTEGER(type) ((type).kind >= TYPE_I8 && (type).kind <= TYPE_INT)
#define IS_REAL(type) ((type).kind >= TYPE_F32 && (type).kind <= TYPE_REAL)
#define IS_NUMBER(type) ((type).kind >= TYPE_I8 && (type).kind <= TYPE_REAL)
#define IS_ADVANCED(type) ((type).kind >= TYPE_STRUCT)

typedef enum {
    TYPE_VOID, TYPE_I8,
    TYPE_CHAR, TYPE_U8,
    TYPE_I16,  TYPE_U16,
    TYPE_I32,  TYPE_U32,
    TYPE_I64,  TYPE_U64,
    TYPE_INT,  TYPE_F32,
    TYPE_F64,  TYPE_REAL,
    TYPE_PTR,  TYPE_STRUCT,
} Type_Kind;

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
    Struct structure;
    Resolve_Status resolve_status;
    Loc loc;
} Advanced_Type;

typedef struct Type Type;
struct Type {
    Advanced_Type *advanced;
    Type_Kind kind : 8;
    Type_Kind deref_kind : 8;
    uint8_t ptr_depth;
};

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

Field *get_first_leaf_field(Struct structure);
Field *get_last_leaf_field(Struct structure);

Type deref_type(Type ptr);

char *type_spelling(Type type);

#endif // ARROW_TYPES_H
