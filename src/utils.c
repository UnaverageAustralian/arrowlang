#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "utils.h"

char *level_str(Level level) {
    switch (level) {
    case LEVEL_NOTE:
        return "\x1b[34mNOTE\x1b[0m";
    case LEVEL_WARN:
        return "\x1b[33mWARNING\x1b[0m";
    case LEVEL_ERR:
        return "\x1b[31mERROR\x1b[0m";
    default:
        fprintf(stderr, "Unknown error level in level_str, please report this as a bug\n");
        exit(1);
    }
}

void eprintf(const char *file_path, int line, int pos, Level level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "%s:%d:%d: %s: ", file_path, line, pos, level_str(level));
    vfprintf(stderr, format, args);

    va_end(args);
}

void sb_appendf(String_Builder *sb, const char *format, ...) {
    va_list args;

    va_start(args, format);
    size_t len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    DA_EXPAND(sb, sb->count + len+1);

    va_start(args, format);
    vsnprintf(sb->items + sb->count, len+1, format, args);
    va_end(args);

    sb->count += len;
}

