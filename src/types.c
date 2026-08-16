#include <stdlib.h>

#include "types.h"

char *type_spelling(Type type) {
    switch (type.kind) {
    case TYPE_VOID: return "void";
    case TYPE_I8:   return "i8";
    case TYPE_CHAR: return "char";
    case TYPE_U8:   return "u8";
    case TYPE_I16:  return "i16";
    case TYPE_U16:  return "u16";
    case TYPE_I32:  return "i32";
    case TYPE_U32:  return "u32";
    case TYPE_I64:  return "i64";
    case TYPE_U64:  return "u64";
    case TYPE_F32:  return "f32";
    case TYPE_F64:  return "f64";
    case TYPE_INT:  return "int";
    case TYPE_REAL: return "real";
    case TYPE_PTR: {
        String_Builder sb = {0};
        for (int i = 0; i < type.ptr_depth; i++)
            sb_appendf(&sb, "ptr[");
        sb_appendf(&sb, "%s", type_spelling((Type){ .kind = type.deref_kind, .advanced = type.advanced }));
        for (int i = 0; i < type.ptr_depth; i++)
            sb_appendf(&sb, "]");
        return sb.items;
    }
    case TYPE_STRUCT: {
        if (type.advanced->structure.name.len == 0)
            return "anon_struct";
        String_Builder sb = {0};
        sb_appendf(&sb, "%.*s", type.advanced->structure.name.len, type.advanced->structure.name.str);
        return sb.items;
    }
    }
    return "unknown";
}

int type_size(Type type) {
    switch (type.kind) {
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
    case TYPE_PTR:
    case TYPE_F64:
    case TYPE_I64:
    case TYPE_U64:
        return 8;
    case TYPE_STRUCT:
        return type.advanced->structure.size;
    default:
        return 0;
    }
}

int typeid(Type type) {
    switch (type.kind) {
    case TYPE_I8:   return 0;
    case TYPE_CHAR: return 1;
    case TYPE_U8:   return 2;
    case TYPE_I16:  return 3;
    case TYPE_U16:  return 4;
    case TYPE_I32:  return 5;
    case TYPE_U32:  return 6;
    case TYPE_INT:
    case TYPE_I64:  return 7;
    case TYPE_U64:  return 8;
    case TYPE_F32:  return 9;
    case TYPE_REAL:
    case TYPE_F64:  return 10;
    default:
        eprintf(__FILE__, (Loc){ .pos = -1, .line = __LINE__ }, LEVEL_ERR, "Invalid type in typeid. This is a bug.\n");
        exit(1);
    }
}

int type_alignment(Type type) {
    if (type.kind == TYPE_STRUCT)
        return type.advanced->structure.alignment;
    else
        return type_size(type);
}

int first_long_is_float(Struct structure) {
    Field *first = structure.fields.items;
    Field *second = first + 1;

    while (IS_ADVANCED(first->type)) {
        Struct *struc = &first->type.advanced->structure;
        if (struc->fields.count > 1)
            second = struc->fields.items + 1;
        first = struc->fields.items;
    }
    while (IS_ADVANCED(second->type))
        second = second->type.advanced->structure.fields.items;

    return first->type.kind == TYPE_F64 || (first->type.kind == TYPE_F32 && second->type.kind == TYPE_F32);
}

int last_long_is_float(Struct structure) {
    Field *last = &structure.fields.items[structure.fields.count - 1];
    Field *second_last = last - 1;

    while (IS_ADVANCED(last->type)) {
        Struct *struc = &last->type.advanced->structure;
        if (struc->fields.count > 1)
            second_last = &struc->fields.items[struc->fields.count - 2];
        last = &struc->fields.items[struc->fields.count - 1];
    }
    while (IS_ADVANCED(second_last->type))
        second_last = second_last->type.advanced->structure.fields.items;

    return structure.size > 8 && (last->type.kind == TYPE_F64 || (last->type.kind == TYPE_F32 && second_last->type.kind == TYPE_F32));
}

int struct_fields_equal(Struct a, Struct b) {
    if (a.fields.count != b.fields.count) return 0;

    for (size_t i = 0; i < a.fields.count; i++)
        if (!types_equal(a.fields.items[i].type, b.fields.items[i].type))
            return 0;
    return 1;
}

int types_compatible(Type a, Type b) {
    if (IS_INTEGER(a))
        return IS_INTEGER(b);
    if (IS_REAL(a))
        return IS_REAL(b);

    if (a.kind != b.kind) return 0;
    if (a.kind == TYPE_PTR)
        return types_equal(deref_type(a), deref_type(b));
    return struct_fields_equal(a.advanced->structure, a.advanced->structure);
}

int types_equal(Type a, Type b) {
    if (a.kind != b.kind) return 0;

    if (a.kind == TYPE_PTR)
        return types_equal(deref_type(a), deref_type(b));
    if (a.kind == TYPE_STRUCT)
        return struct_fields_equal(a.advanced->structure, b.advanced->structure);
    return 1;
}

Type deref_type(Type ptr) {
    if (ptr.kind != TYPE_PTR) return ptr;

    ptr.ptr_depth--;
    if (ptr.ptr_depth == 0)
        return (Type){ .kind = ptr.deref_kind, .advanced = ptr.advanced };
    else
        return ptr;
}

