#ifndef ARROW_UTILS_H
#define ARROW_UTILS_H

#include <stddef.h>

#define INITIAL_CAPACITY 16

#define DA_EXPAND(a, n)                                                            \
    do {                                                                           \
        if (n > (a)->capacity) {                                                   \
            if ((a)->capacity == 0) (a)->capacity = INITIAL_CAPACITY;              \
            while (n > (a)->capacity)                                              \
                (a)->capacity *= 2;                                                \
            (a)->items = realloc((a)->items, (a)->capacity * sizeof(*(a)->items)); \
            assert((a)->items != NULL);                                            \
        }                                                                          \
    } while (0)

#define DA_APPEND(a, i)                 \
    do {                                \
        DA_EXPAND(a, (a)->count+1);     \
        (a)->items[(a)->count++] = (i); \
    } while (0)

typedef enum {
    LEVEL_NOTE,
    LEVEL_WARN,
    LEVEL_ERR,
} Level;

typedef struct {
    size_t count;
    size_t capacity;
    char *items;
} String_Builder;

void eprintf(const char *file_path, int line, int pos, Level level, const char *format, ...);

void sb_appendf(String_Builder *sb, const char *format, ...);

#endif // ARROW_UTILS_H
