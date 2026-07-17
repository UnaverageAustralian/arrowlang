#ifndef ARROW_TYPE_CHECKING_H
#define ARROW_TYPE_CHECKING_H

#include "compiler.h"

typedef struct {
    Types stack;
    Types expected_types;
    Ops dst;
    Function *func;
    Ops *ops;
    unsigned block_start;
    unsigned expected_types_start;
    unsigned pos;
    int allocated;
    int max_allocated;
    uint8_t had_error;
    uint8_t in_block;
} Analyser;

int type_check(Ops *ops);

#endif // ARROW_TYPE_CHECKING_H
