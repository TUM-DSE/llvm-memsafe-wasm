#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75

// TODO: when testing, make sure to disable assertions at runtime with -DNDEBUG
typedef struct {
    void *key;
    // The memory region size a user requests.
    size_t requested_size;
    // The memory region size that is actually allocated internally, which
    // may be larger than requested_size due to alignment constraints.
    size_t allocated_size;
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
            if (entry->allocated_size == 0) {
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
        entries[i].requested_size = 0;
        entries[i].allocated_size = 0;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; ++i) {
        TableEntry *entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }

        TableEntry *dest = __wasm_memsafety_table_find(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->requested_size = entry->requested_size;
        dest->allocated_size = entry->allocated_size;
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

static bool __wasm_memsafety_table_set(HashTable *table, void *ptr, size_t requested_size, size_t allocated_size) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        size_t capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        __wasm_memsafety_table_resize(table, capacity);
    }

    TableEntry *entry = __wasm_memsafety_table_find(table->entries, table->capacity, ptr);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && entry->allocated_size == 0) {
        table->count++;
    }

    entry->key = ptr;
    entry->requested_size = requested_size;
    entry->allocated_size = allocated_size;
    return isNewKey;
}

static TableEntry __wasm_memsafety_table_remove(HashTable *table, void *ptr) {
    if (table->count == 0) {
        return (TableEntry) {NULL, 0, 0};
    }

    TableEntry *entry = __wasm_memsafety_table_find(table->entries, table->capacity, ptr);
    if (entry->key == NULL) {
        return (TableEntry) {NULL, 0, 0};
    }

    TableEntry result = *entry;

    // Place a tombstone in the entry
    entry->key = NULL;
    entry->requested_size = 0;
    entry->allocated_size = 1;

    return result;
}

static HashTable table = {.capacity = 0, .count = 0, .entries = NULL};


// The descriptions for malloc, calloc, realloc and free have been taken from
// the malloc(3) linux manual page.

#define MTE_ALIGNMENT 16
#define STRIP_MTE_TAG(ptr) ((void *) ((size_t) (ptr) & 0xF0FFFFFFFFFFFFFF))


// Calculate smallest multiple of align that is not less than size
static inline size_t __wasm_memsafety_align_size(size_t align, size_t size) {
    return (size + (align - 1)) & (~(align - 1));
}

// A custom aligned_alloc implementation that does not enforce the strict
// requirement that (requested_size % aligment == 0). But it always aligns
// the size to a multiple of MTE_ALIGNMENT, to guarantee MTE correctness.
void *__wasm_memsafety_aligned_alloc_for_mte(size_t alignment, size_t requested_size) {
    if (requested_size == 0) {
        return NULL;
    }

    // Alignment has to be at least MTE_ALIGNMENT bytes.
    alignment = alignment > MTE_ALIGNMENT ? alignment : MTE_ALIGNMENT;
    size_t aligned_size = __wasm_memsafety_align_size(alignment, requested_size);

    void *mem = aligned_alloc(alignment, aligned_size);
    if (mem) {
        mem = __builtin_wasm_segment_new_stack(mem, aligned_size);
        // NOTE: remember to remove for benchmarking
        fprintf(stderr, "Tagging memory %p, requested size %zu, allocated size %zu\n", mem, requested_size, aligned_size);

        __wasm_memsafety_table_set(&table, mem, requested_size, aligned_size);
    }

    return mem;
}

// The obsolete function memalign() allocates size bytes and returns a pointer
// to the allocated memory. The memory address will be a multiple of alignment,
// which must be a power of two. The function aligned_alloc() is the same as
// memalign(), except for the added restriction that size should be a multiple
// of alignment.
void *__wasm_memsafety_aligned_alloc(size_t alignment, size_t requested_size) {
    // aligned_alloc(3) defines that size must be a multiple of the alignment.
    if (requested_size % alignment != 0) {
      return NULL;
    }

    return __wasm_memsafety_aligned_alloc_for_mte(alignment, requested_size);
}

// The malloc() function allocates size bytes and returns a pointer to the
// allocated memory. The memory is not initialized. If size is 0, then malloc()
// returns either NULL, or a unique pointer value that can later be
// successfully passed to free().
void *__wasm_memsafety_malloc(size_t requested_size) {
    return __wasm_memsafety_aligned_alloc_for_mte(MTE_ALIGNMENT, requested_size);
}

// Check if multiplying a and b would overflow.
static inline bool __wasm_memsafety_check_multiplication_overflow(size_t a, size_t b) {
    // We want to check if `a * b > SIZE_MAX`, which can be mathematically
    // transformed to `b > SIZE_MAX / a`
    return (a != 0 && b > (SIZE_MAX / a));
}

// The calloc() function allocates memory for an array of nmemb elements of size
// bytes each and returns a pointer to the allocated memory. The memory is set to
// zero. If nmemb or size is 0, then calloc() returns either NULL, or a unique
// pointer value that can later be successfully passed to free().
void *__wasm_memsafety_calloc(size_t nmemb, size_t size) {
    if (__wasm_memsafety_check_multiplication_overflow(nmemb, size)) {
        return NULL;
    }
    size_t requested_size = nmemb * size;

    if (requested_size == 0) {
        return NULL;
    }

    void *mem = __wasm_memsafety_malloc(requested_size);
    if (mem) {
        // We only set the requested number of bytes to 0, not any extra bytes
        // that malloc might have allocated due to alignment.
        memset(mem, 0, requested_size);
    }

    return mem;
}

static inline size_t __wasm_memsafety_min(size_t a, size_t b) {
    return a < b ? a : b;
}

// The free() function frees the memory space pointed to by ptr,
// which must have been returned by a previous call to malloc() or
// related functions.  Otherwise, or if ptr has already been freed,
// undefined behavior occurs.  If ptr is NULL, no operation is
// performed.
void __wasm_memsafety_free(void *ptr) {
    TableEntry entry = __wasm_memsafety_table_remove(&table, ptr);
    if (entry.key != NULL) {
        // NOTE: remember to remove for benchmarking
        fprintf(stderr, "Untagging memory %p, requested size %zu, allocated size %zu\n", entry.key, entry.requested_size, entry.allocated_size);
        __builtin_wasm_segment_free(entry.key, entry.allocated_size);
        void *untagged_ptr = STRIP_MTE_TAG(entry.key);
        free(untagged_ptr);
    } else {
        // This branch should only be entered when `free(NULL)` is called, which
        // is safe and defined in the C standard.
        free(ptr);
    }
}

// TODO: what should happen if we call realloc on a ptr that was previously called with aligned_alloc? Do we have to follow that original alignment?
// TODO: we're not really setting errno's correctly, should we do that?

// The realloc() function changes the size of the memory block pointed to by ptr
// to size bytes. The contents will be unchanged in the range from the start of
// the region up to the minimum of the old and new sizes. If the new size is larger
// than the old size, the added memory will not be initialized. If ptr is NULL,
// then the call is equivalent to malloc(size), for all values of size; if size is
// equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
// Unless ptr is NULL, it must have been returned by an earlier call to malloc(),
// calloc() or realloc(). If the area pointed to was moved, a free(ptr) is done.
void *__wasm_memsafety_realloc(void *ptr, size_t requested_size) {
    // malloc(3): If ptr is NULL, then the call is equivalent to malloc(size),
    // for all values of size.
    if (ptr == NULL) {
        return __wasm_memsafety_malloc(requested_size);
    }

    // malloc(3): If size is equal to zero, and ptr is not NULL, then the call is
    // equivalent to free(ptr).
    if (requested_size == 0) {
        __wasm_memsafety_free(ptr);
        return NULL;
    }
    
    TableEntry *entry = __wasm_memsafety_table_find(table.entries, table.capacity, ptr);
    if (entry->key == NULL) {
        // The pointer wasn't found in the table, indicating the problem that
        // the ptr was never created with a previous malloc, calloc or realloc.
        return NULL;
    }

    // TODO: maybe explicitly do nothing if requested_size == allocated_size, though for now this handles that case as well
    // TODO: this assumes an alignment of 16 bytes, but that is not guaranteed (but changing alignment should be allowed since it's undefined in C standard)
    // If the requested_size is in the last MTE_ALIGNMENT-wide block that was allocated
    if ((entry->allocated_size - MTE_ALIGNMENT) < requested_size && requested_size <= entry->allocated_size) {
        // Just update requested_size
        __wasm_memsafety_table_set(&table, ptr, requested_size, entry->allocated_size);
        return ptr;
    }

    // Allocate new memory, copy the data, and free the old memory
    void *new_ptr = __wasm_memsafety_malloc(requested_size);
    if (new_ptr) {
        size_t copied_size = __wasm_memsafety_min(requested_size, entry->requested_size);
        memcpy(new_ptr, ptr, copied_size);
        __wasm_memsafety_free(ptr);
    }

    return new_ptr;
}
