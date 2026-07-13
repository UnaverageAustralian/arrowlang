#include "types.h"

int struct_size(Struct structure) {
    int result = 0;
    for (size_t i = 0; i < structure.fields.count; i++) {
        result = ALIGN(result, type_alignment(structure.fields.items[i].type));
        if (structure.fields.items[i].type.kind == KIND_STRUCT)
            result += struct_size(structure.fields.items[i].type.as.structure);
        else
            result += type_size(structure.fields.items[i].type);
    }
    return result;
}

int type_size(Type type) {
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
        return struct_size(type.as.structure);
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

int type_alignment(Type type) {
    switch (type.kind) {
    case KIND_BASIC:  return type_size(type);
    case KIND_STRUCT: return type.as.structure.alignment;
    default:          return 0;
    }
}

Field *get_first_leaf_field(Struct structure) {
    Field *result = &structure.fields.items[0];
    while (result->type.kind == KIND_STRUCT)
        result = &result->type.as.structure.fields.items[0];
    return result;
}

Field *get_last_leaf_field(Struct structure) {
    Field *result = &structure.fields.items[0];
    while (result->type.kind == KIND_STRUCT)
        result = &result->type.as.structure.fields.items[result->type.as.structure.fields.count-1];
    return result;
}

int struct_fields_equal(Struct a, Struct b) {
    if (a.fields.count != b.fields.count) return 0;

    for (size_t i = 0; i < a.fields.count; i++)
        if (!types_equal(a.fields.items[i].type, b.fields.items[i].type))
            return 0;
    return 1;
}

int types_compatible(Type a, Type b) {
    if (a.kind != b.kind) return 0;

    switch (a.kind) {
    case KIND_BASIC:  return (a.as.basic & b.as.basic) != 0;
    case KIND_STRUCT: return struct_fields_equal(a.as.structure, a.as.structure);
    default:          return 0;
    }
}

int types_equal(Type a, Type b) {
    if (a.kind != b.kind) return 0;

    switch (a.kind) {
    case KIND_BASIC:  return a.as.basic == b.as.basic;
    case KIND_STRUCT: return struct_fields_equal(a.as.structure, a.as.structure);
    default:          return 0;
    }
}

