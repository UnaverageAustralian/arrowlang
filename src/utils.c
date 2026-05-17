#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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

static uint64_t hash_str(const char *key, size_t key_len) {
    uint64_t hash = 0xCBF29CE484222325;
    for (size_t i = 0; i < key_len; i++) {
        hash ^= key[i];
        hash *= 0x100000001B3;
    }
    return hash;
}

Hash_Entry *hashmap_get(Hashmap *map, const char *key, size_t key_len) {
    assert(map->entries != NULL);
    uint64_t index = hash_str(key, key_len) & (map->capacity-1);

    for (size_t i = 0; i < map->capacity; i++) {
        Hash_Entry *entry = &map->entries[(index + i) & (map->capacity-1)];
        if ((entry->key_len == key_len && strncmp(entry->key, key, key_len) == 0) || entry->key == NULL)
            return entry;
    }
    return NULL;
}

static void expand_hashmap(Hashmap *map) {
    Hashmap new = {0};
    new.capacity = map->capacity == 0 ? 16 : map->capacity*2;
    new.entries = calloc(new.capacity, sizeof(Hash_Entry));

    for (size_t i = 0; i < map->capacity; i++) {
        Hash_Entry *entry = &map->entries[i];
        if (entry->key != NULL)
            hashmap_add(&new, entry->key, entry->key_len, entry->val);
    }
    free(map->entries);
    *map = new;
}

Hash_Entry *hashmap_add(Hashmap *map, const char *key, size_t key_len, void *val) {
    if (map->count >= map->capacity/2)
        expand_hashmap(map);

    if (hashmap_get(map, key, key_len)->key) return NULL;

    uint64_t index = hash_str(key, key_len) & (map->capacity-1);
    while (map->entries[index].key != NULL) {
        index++;
        index &= map->capacity-1;
    }

    map->entries[index] = (Hash_Entry){ .key = key, .key_len = key_len, .val = val };
    map->count++;
    return &map->entries[index];
}

void init_arena(Arena *arena, size_t size) {
    arena->capacity = size;
    arena->allocated = 0;
    arena->block = malloc(size);
    assert(arena->block != NULL);
}

void *arena_calloc(Arena *arena, size_t num_bytes) {
    if (arena->allocated + num_bytes >= arena->capacity) {
        fprintf(stderr, "\x1b[31mERROR\x1b[0m: The compiler has run out of memory\n");
        exit(1);
    }
    memset((uint8_t *)arena->block + arena->allocated, 0, num_bytes);
    void *ptr = (uint8_t *)arena->block + arena->allocated;
    arena->allocated += num_bytes;
    return ptr;
}

void free_arena(Arena *arena) {
    arena->allocated = 0;
    arena->capacity = 0;
    free(arena->block);
}

