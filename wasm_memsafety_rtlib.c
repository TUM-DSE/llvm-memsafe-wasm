#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define TABLE_MAX_LOAD 0.75

typedef struct {
    void *key;
    size_t size;
} TableEntry;


typedef struct {
    size_t count;
    size_t capacity;
    TableEntry *entries;
} HashTable;

static TableEntry *__wasm_memsafety_table_find(TableEntry *entries, size_t capacity, void *ptr) {
    size_t index = (size_t) ptr % capacity;
    TableEntry *tombstone = NULL;

    for (;;) {
        TableEntry *entry = &entries[index];

        if (entry->key == NULL) {
            if (entry->size == 0) {
                // found empty entry
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                // found tombstone
                tombstone = entry;
            }
        } else if (entry->key == ptr) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void __wasm_memsafety_table_resize(HashTable *table, size_t capacity) {
    TableEntry *entries = calloc(capacity, sizeof(TableEntry));
    for (int i = 0; i < capacity; ++i) {
        entries[i].key = NULL;
        entries[i].size = 0;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; ++i) {
        TableEntry *entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }

        TableEntry *dest = __wasm_memsafety_table_find(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->size = entry->size;
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

static bool __wasm_memsafety_table_set(HashTable *table, void *ptr, size_t size) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        size_t capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        __wasm_memsafety_table_resize(table, capacity);
    }

    TableEntry *entry = __wasm_memsafety_table_find(table->entries, table->capacity, ptr);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && entry->size == 0) {
        table->count++;
    }

    entry->key = ptr;
    entry->size = size;
    return isNewKey;
}

static TableEntry __wasm_memsafety_table_remove(HashTable *table, void *ptr) {
    if (table->count == 0) {
        return (TableEntry) {NULL, 0};
    }

    TableEntry *entry = __wasm_memsafety_table_find(table->entries, table->capacity, ptr);
    if (entry->key == NULL) {
        return (TableEntry) {NULL, 0};
    }

    TableEntry result = *entry;

    // Place a tombstone in the entry
    entry->key = NULL;
    entry->size = 1;

    return result;
}

static HashTable table = {.capacity = 0, .count = 0, .entries = NULL};

void *__wasm_memsafety_malloc(size_t align, size_t size) {
    align = align > 16 ? align : 16;
    size_t aligned_size = (size + (align - 1)) & (~(align - 1));
    void *mem = aligned_alloc(align, aligned_size);
    if (mem) {
        mem = __builtin_wasm_segment_new_stack(mem, aligned_size);
        fprintf(stderr, "Tagging memory %p, size %zu\n", mem, aligned_size);

        __wasm_memsafety_table_set(&table, mem, aligned_size);
    }

    return mem;
}

void __wasm_memsafety_free(void *ptr) {
    TableEntry entry = __wasm_memsafety_table_remove(&table, ptr);
    if (entry.key != NULL) {
        fprintf(stderr, "Untagging memory %p, size %zu\n", entry.key, entry.size);
        __builtin_wasm_segment_free(entry.key, entry.size);
        void *untagged_ptr = (void *) ((size_t) entry.key & 0xF0FFFFFFFFFFFFFF);
        free(untagged_ptr);
    } else {
        free(ptr);
    }
}
