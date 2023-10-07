#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#define MTE_ALIGNMENT 16
#define MTE_NON_TAG_BITS_MASK 0xF0FFFFFFFFFFFFFF

typedef struct {
  // The size of the (tagged) memory region accessible to the user. The size
  // (of the memory) required to store this allocation metadata is excluded.
  size_t tagged_size;
} AllocMetadata;

// Size of aligned AllocMetadata.
typedef struct {
  // We have to respect a custom alignment value specified by the user when
  // storing the metadata right before the user memory. We need to know the
  // alignment before accessing the metadata, because: How is our free function
  // supposed to know how far back to look to access the metadata? This depends
  // on the alignment.
  size_t metadata_size;
} AllocMetadataSize;

//          alignment             alignment
// +----------------------+-----------------------+-------------------
// | AllocMetadata        |     AllocMetadataSize | user memory ...
// +----------------------+-----------------------+-------------------

// NOTE: both of the structs could also be stored in the same alignment-block,
// this graphic only showcases the order, and that the AllocAlignmentMetadata
// comes directly before the start of the user memory, so the free() function
// knows exactly where to look for it (right before its start).

/**
 * Align a value to a given alignment.
 * @param val
 * @param align
 * @return the smallest value that is a multiple of align and greater or equal
 * to val
 */
static inline size_t __wasm_memsafety_align(size_t val, size_t align) {
  return (val + (align - 1)) & (~(align - 1));
}

static inline void *__wasm_memsafety_untag_ptr(void *ptr) {
  return (void *)((size_t)ptr & MTE_NON_TAG_BITS_MASK);
}

/**
 * Save the metadata for a given pointer.
 * @param mem the pointer used internally that addresses the metadata and the
 * user memory
 * @param untagged_user_ptr the raw pointer given to the user
 * @param metadata the metadata to save
 * @param metadata_size the metadata size to save
 */
static inline void
__wasm_memsafety_save_all_metadata(void *mem, void *untagged_user_ptr,
                                   AllocMetadata metadata,
                                   AllocMetadataSize metadata_size) {
  // Store metadata right at the beginning of the metadata block.
  void *metadata_ptr = mem;
  *((AllocMetadata *)metadata_ptr) = metadata;

  // Store metadata size right at the end of the metadata block.
  char *metadata_size_ptr =
      (char *)untagged_user_ptr - sizeof(AllocMetadataSize);
  *((AllocMetadataSize *)metadata_size_ptr) = metadata_size;
}

/**
 * Get the size metadata for a given pointer.
 * @param untagged_user_ptr the raw pointer given to the user
 * @return the pointer to the tagged_size metadata, which is also the internal
 * pointer (addresses the entire allocation, so can be used for freeing)
 */
static inline AllocMetadata *
__wasm_memsafety_get_metadata(void *untagged_user_ptr) {
  // Get the alignment metadata first. Here, we assume it was saved directly
  // before the user pointer.
  char *metadata_size_ptr =
      (char *)untagged_user_ptr - sizeof(AllocMetadataSize);
  size_t metadata_size =
      ((AllocMetadataSize *)metadata_size_ptr)->metadata_size;
  char *metadata_ptr = (char *)untagged_user_ptr - metadata_size;

  return (AllocMetadata *)(metadata_ptr);
}

// A custom aligned_alloc implementation that does not enforce the strict
// requirement that (requested_size % aligment == 0). But it always aligns
// the size to a multiple of MTE_ALIGNMENT, to guarantee MTE correctness.
void *__wasm_memsafety_aligned_alloc_for_mte(size_t alignment,
                                             size_t requested_size) {
  if (requested_size == 0) {
    return NULL;
  }

  // Since the alignment value must be a power of 2, `align` will always be a
  // multiple of MTE_ALIGNMENT.
  alignment = alignment > MTE_ALIGNMENT ? alignment : MTE_ALIGNMENT;

  // The size of the region we want to tag with MTE.
  size_t tagged_size = __wasm_memsafety_align(requested_size, alignment);

  // We don't need the metadata and its size to be aligned separately. Instead,
  // we increase compactness by requiring that only the addition of their
  // sizes is aligned.
  size_t metadata_size = __wasm_memsafety_align(
      sizeof(AllocMetadata) + sizeof(AllocMetadataSize), alignment);

  size_t total_size = metadata_size + tagged_size;

  void *mem = aligned_alloc(alignment, total_size);
  if (!mem) {
    return NULL;
  }

  // Transform mem into the form we want to pass to user, i.e. hide our
  // embedded metadata.
  void *untagged_user_ptr = (void *)((char *)mem + metadata_size);

  // Save metadata (tagged_size and the metadata's size)
  AllocMetadata metadata = {.tagged_size = tagged_size};
  AllocMetadataSize metadata_size_ = {.metadata_size = metadata_size};
  __wasm_memsafety_save_all_metadata(mem, untagged_user_ptr, metadata,
                                     metadata_size_);

  void *tagged_user_ptr =
      __builtin_wasm_segment_new(untagged_user_ptr, tagged_size);
  DEBUG_PRINT("Tagging memory %p, size %zu\n", tagged_user_ptr,
          tagged_size);

  return tagged_user_ptr;
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
static inline bool __wasm_memsafety_check_multiplication_overflow(size_t a,
                                                                  size_t b) {
  // We want to check if `a * b > SIZE_MAX`, which can be mathematically
  // transformed to `b > SIZE_MAX / a`
  return (a != 0 && b > (SIZE_MAX / a));
}

// The calloc() function allocates memory for an array of nmemb elements of size
// bytes each and returns a pointer to the allocated memory. The memory is set
// to zero. If nmemb or size is 0, then calloc() returns either NULL, or a
// unique pointer value that can later be successfully passed to free().
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
void __wasm_memsafety_free(void *tagged_ptr) {
  if (tagged_ptr == NULL) {
    return;
  }

  // Get tagged_size
  void *untagged_ptr = __wasm_memsafety_untag_ptr(tagged_ptr);
  AllocMetadata *metadata = __wasm_memsafety_get_metadata(untagged_ptr);
  size_t tagged_size = metadata->tagged_size;

  DEBUG_PRINT("Untagging memory %p, size %zu\n", tagged_ptr, tagged_size);
  __builtin_wasm_segment_free(tagged_ptr, tagged_size);

  // We stored the metadata at the beginning of the total allocation, so we
  // use that.
  void *mem = (void *)metadata;
  free(mem);
}

// The realloc() function changes the size of the memory block pointed to by ptr
// to size bytes. The contents will be unchanged in the range from the start of
// the region up to the minimum of the old and new sizes. If the new size is
// larger than the old size, the added memory will not be initialized. If ptr is
// NULL, then the call is equivalent to malloc(size), for all values of size; if
// size is equal to zero, and ptr is not NULL, then the call is equivalent to
// free(ptr). Unless ptr is NULL, it must have been returned by an earlier call
// to malloc(), calloc() or realloc(). If the area pointed to was moved, a
// free(ptr) is done.
void *__wasm_memsafety_realloc(void *tagged_ptr, size_t requested_size) {
  // malloc(3): If ptr is NULL, then the call is equivalent to malloc(size),
  // for all values of size.
  if (tagged_ptr == NULL) {
    return __wasm_memsafety_malloc(requested_size);
  }

  // malloc(3): If size is equal to zero, and ptr is not NULL, then the call is
  // equivalent to free(ptr).
  if (requested_size == 0) {
    __wasm_memsafety_free(tagged_ptr);
    return NULL;
  }

  void *untagged_ptr = __wasm_memsafety_untag_ptr(tagged_ptr);
  AllocMetadata *metadata = __wasm_memsafety_get_metadata(untagged_ptr);

  // if the requested and current size are equal, do nothing
  if (__wasm_memsafety_align(requested_size, MTE_ALIGNMENT) == metadata->tagged_size) {
    return tagged_ptr;
  }

  // Allocate new memory, copy the data, and free the old memory
  void *new_ptr = __wasm_memsafety_malloc(requested_size);
  if (new_ptr) {
    size_t copied_size =
        __wasm_memsafety_min(requested_size, metadata->tagged_size);
    memcpy(new_ptr, tagged_ptr, copied_size);
    __wasm_memsafety_free(tagged_ptr);
  }

  return new_ptr;
}
