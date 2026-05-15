#ifndef ARROW_GEN_H
#define ARROW_GEN_H

#include "compiler.h"
#include "utils.h"

typedef struct {
    String_Builder sb;
    Ops *ops;
    int had_error;
} Generator;

void generate_x86_64_linux(Ops *ops);

#endif // ARROW_GEN_H
