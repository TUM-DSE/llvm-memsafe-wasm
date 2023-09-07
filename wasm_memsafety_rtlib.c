#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MTE_ALIGNMENT 16

typedef struct {
  size_t size;
} AllocMetadata;

/**
 * Align a value to a given alignment.
 * @param val
 * @param align
 * @return a value that is a multiple of align and greater or equal to val
 */
static inline size_t __wasm_memsafety_align(size_t val, size_t align) {
  return (val + (align - 1)) & (~(align - 1));
}

static inline void *__wasm_memsafety_untag_ptr(void *ptr) {
  return (void *) ((size_t) ptr & 0xF0FFFFFFFFFFFFFF);
}

/**
 * Get the metadata for a given pointer.
 * @param ptr the raw pointer given to the user
 * @return the metadata corresponding to the given pointer
 */
static inline AllocMetadata *__wasm_memsafety_metadata(void *ptr) {
  size_t meta_size = __wasm_memsafety_align(sizeof(AllocMetadata), MTE_ALIGNMENT);
  return (AllocMetadata *) (ptr - meta_size);
}

void *__wasm_memsafety_malloc(size_t size) {
  size_t meta_size = __wasm_memsafety_align(sizeof(AllocMetadata), MTE_ALIGNMENT);
  size_t aligned_size = __wasm_memsafety_align(size, MTE_ALIGNMENT);
  size_t total_size = meta_size + aligned_size;
  void *mem = aligned_alloc(MTE_ALIGNMENT, total_size);

  if (!mem) {
    return NULL;
  }

  void *tagged_mem = mem + meta_size;
  AllocMetadata *meta = __wasm_memsafety_metadata(tagged_mem);
  meta->size = total_size;

  tagged_mem = __builtin_wasm_segment_new_stack(tagged_mem, aligned_size);
  fprintf(stderr, "Tagging memory %p, size %zu\n", tagged_mem, aligned_size);

  return tagged_mem;
}

void __wasm_memsafety_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  // since ptr is tagged, we first need to remove that tag before we access the metadata, since
  // the metadata is not tagged.
  AllocMetadata *meta = __wasm_memsafety_metadata(__wasm_memsafety_untag_ptr(ptr));
  size_t total_size = meta->size;
  size_t tagged_size = (total_size - __wasm_memsafety_align(sizeof(AllocMetadata), MTE_ALIGNMENT));
  // the metadata is the beginning of the allocation, so we use that
  void *mem = (void *) meta;

  fprintf(stderr, "Untagging memory %p, size %zu\n", ptr, tagged_size);
  __builtin_wasm_segment_free(ptr, tagged_size);
  free(mem);
}
