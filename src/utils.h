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

#define ARENA_DA_APPEND(arena, a, i)                                                  \
    do {                                                                              \
        if ((a)->capacity == 0) {                                                     \
            (a)->items = arena_calloc(arena, INITIAL_CAPACITY * sizeof(*(a)->items)); \
            (a)->capacity = INITIAL_CAPACITY;                                         \
        }                                                                             \
        else if ((a)->count >= (a)->capacity) {                                       \
            arena_calloc(arena, (a)->capacity * sizeof(*(a)->items));                 \
            (a)->capacity *= 2;                                                       \
        }                                                                             \
        (a)->items[(a)->count++] = (i);                                               \
    } while (0)

#define ALIGN(a, b) (a) + ((b) - 1) - (((a) - 1) & ((b) - 1))

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

typedef struct {
    const char *key;
    size_t key_len;
    void *val;
} Hash_Entry;

typedef struct {
    size_t count;
    size_t capacity;
    Hash_Entry *entries;
} Hashmap;

typedef struct {
    size_t allocated;
    size_t capacity;
    void *block;
} Arena;

typedef struct {
    size_t count;
    size_t capacity;
    const char **items;
} Cmd;

typedef Cmd String_Array;

typedef struct {
    size_t len;
    const char *str;
} String_View;

typedef struct {
    int pos;
    int line;
} Loc;

void eprintf(const char *file_path, Loc loc, Level level, const char *format, ...);
void sb_appendf(String_Builder *sb, const char *format, ...);

void cmd_append_many(Cmd *cmd, int argc, ...);
int cmd_exec(Cmd *cmd);

Hash_Entry *hashmap_add(Hashmap *map, const char *key, size_t key_len, void *val);
Hash_Entry *hashmap_get(Hashmap *map, const char *key, size_t key_len);

void init_arena(Arena *arena, size_t size);
void *arena_calloc(Arena *arena, size_t num_bytes);
void free_arena(Arena *arena);

char *open_file(char *path);

#endif // ARROW_UTILS_H
