#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

void usage(char *file) {
    fprintf(stderr, "Usage: %s [<options>] <file>\n", file);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "   -o <file>       - Output file\n");
    fprintf(stderr, "\n");
}

Compiler_Options parse_arguments(int argc, char **argv) {
    Compiler_Options options = {0};

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }

    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            i++;
            if (i == argc) {
                usage(argv[0]);
                fprintf(stderr, "ERROR: Expected argument after -o\n");
                exit(1);
            }
            if (options.output_file) {
                fprintf(stderr, "ERROR: -o cannot be used multiple times\n");
                exit(1);
            }
            options.output_file = argv[i];
        }
        else {
            usage(argv[0]);
            fprintf(stderr, "ERROR: Invalid option: %s\n", argv[i]);
            exit(1);
        }
    }

    if (i == argc) {
        usage(argv[0]);
        fprintf(stderr, "ERROR: Expected input file\n");
        exit(1);
    }

    options.input_files = argv + i;

    if (options.output_file == NULL)
        options.output_file = "a";
    return options;
}

int main(int argc, char **argv) {
    Compiler_Options options = parse_arguments(argc, argv);
    compile(options);
    return 0;
}
