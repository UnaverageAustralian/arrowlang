#ifndef ARROW_GEN_H
#define ARROW_GEN_H

#include "compiler.h"
#include "utils.h"

typedef struct {
    String_Builder sb;
    String_Array strs;
    Globals globals;
    Function func;
    Ops *ops;
    unsigned pos;
    int allocated;
    int had_error;
} Generator;

char *generate_x86_64(Ops *ops, char *output_file, int gen_start);

#endif // ARROW_GEN_H
