#ifndef ARROW_TYPE_CHECKING_H
#define ARROW_TYPE_CHECKING_H

#include "compiler.h"

typedef struct {
    Types stack;
    Types expected_types;
    Function func;
    Ops dst;
    Ops *ops;
    unsigned block_start;
    unsigned expected_types_start;
    unsigned pos;
    uint8_t had_error;
    uint8_t in_block;
} Analyser;

int type_check(Ops *ops);
char *type_spelling(Type type);

#endif // ARROW_TYPE_CHECKING_H
