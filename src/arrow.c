#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

void usage(char *file) {
    fprintf(stderr, "Usage: %s [<options>] <files> [<options>]\n", file);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "   -o <file>       - Output file\n");
    fprintf(stderr, "   -L <path>       - Add path to linker search paths\n");
    fprintf(stderr, "   -l <library>    - Link with library\n");
    fprintf(stderr, "\n");
}

int parse_flags(Compiler_Options *options, int argc, char **argv, int i) {
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            i++;
            if (i == argc) {
                usage(argv[0]);
                fprintf(stderr, "ERROR: Expected argument after -o\n");
                exit(1);
            }
            if (options->output_file) {
                fprintf(stderr, "ERROR: -o cannot be used multiple times\n");
                exit(1);
            }
            options->output_file = argv[i];
        }
        else if (strncmp(argv[i], "-L", 2) == 0 || strncmp(argv[i], "-l", 2) == 0) {
            if (argv[i][2] == '\0' && i == argc) {
                usage(argv[0]);
                fprintf(stderr, "ERROR: Expected argument after %2s\n", argv[i]);
                exit(1);
            }

            DA_APPEND(&options->link_cmd, argv[i]);
            if (argv[i][2] == '\0') {
                i++;
                DA_APPEND(&options->link_cmd, argv[i]);
            }
        }
        else {
            usage(argv[0]);
            fprintf(stderr, "ERROR: Invalid option: %s\n", argv[i]);
            exit(1);
        }
    }
    return i;
}

Compiler_Options parse_arguments(int argc, char **argv) {
    Compiler_Options options = {0};

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }

    int i = parse_flags(&options, argc, argv, 1);
    if (i == argc) {
        usage(argv[0]);
        fprintf(stderr, "ERROR: Expected input file\n");
        exit(1);
    }

    options.input_files = argv + i;

    int count = 0;
    for (; argv[i][0] != '-'; i++)
        count++;
    options.input_file_count = count;

    i = parse_flags(&options, argc, argv, i);
    if (argv[i] != NULL) {
        usage(argv[0]);
        fprintf(stderr, "ERROR: Extra arguments at the end\n");
        exit(1);
    }

    if (options.output_file == NULL)
        options.output_file = "a";
    return options;
}

int main(int argc, char **argv) {
    Compiler_Options options = parse_arguments(argc, argv);
    compile(options);
    return 0;
}
