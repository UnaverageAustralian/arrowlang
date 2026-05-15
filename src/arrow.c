#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "compiler.h"

char *open_file(char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) == -1) return NULL;

    long long length = ftell(f);
    if (length == -1) return NULL;
    rewind(f);

    char *contents = malloc(length+1);
    if (!contents) return NULL;

    fread(contents, 1, length, f);
    if (ferror(f)) return NULL;
    fclose(f);

    contents[length] = '\0';
    return contents;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    char *contents = open_file(argv[1]);
    if (!contents) {
        fprintf(stderr, "\x1b[31mERROR:\x1b[0m Could not read file: %s\n", strerror(errno));
        return 1;
    }

    compile(contents, argv[1]);

    free(contents);
    return 0;
}
