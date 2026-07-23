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
    fprintf(stderr, "   -v              - Show all commands executed\n");
    fprintf(stderr, "   -S              - Emit assembly files instead of an executable\n");
    fprintf(stderr, "   -C              - Emit object files instead of an executable\n");
    fprintf(stderr, "   -debug          - Only lex and compile then print IR afterwards\n");
    fprintf(stderr, "   -print-ir       - Only lex, compile, and analyse then print IR afterwards\n");
    fprintf(stderr, "   --help          - Display this\n");
    fprintf(stderr, "   --version       - Display compiler version\n");
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
            options->output_file = argv[i];
        }
        else if (strcmp(argv[i], "-v") == 0) {
            options->verbose = 1;
        }
        else if (strcmp(argv[i], "-S") == 0) {
            options->emit_asm = 1;
        }
        else if (strcmp(argv[i], "-C") == 0) {
            options->emit_obj = 1;
        }
        else if (strcmp(argv[i], "-debug") == 0) {
            options->debug = 1;
        }
        else if (strcmp(argv[i], "-print-ir") == 0) {
            options->print_ir = 1;
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
        else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        }
        else if (strcmp(argv[i], "--version") == 0) {
            printf("Version: 0.26.0\n");
            exit(0);
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
    for (; i < argc && argv[i][0] != '-'; i++)
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
