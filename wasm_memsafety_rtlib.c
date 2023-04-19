#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef uint64_t tagged_ptr_t;

struct Node {
    struct Node *next;
    void *original_ptr;
    tagged_ptr_t tagged_ptr;
    size_t size;
};

typedef struct Node Node;

static struct Node *head = NULL;


/// Find and remove a node in the list
struct Node *__wasm_memsafety_find(Node **head, tagged_ptr_t ptr) {
    Node **curr = head;
    while (*curr) {
        if ((*curr)->tagged_ptr == ptr) {
            Node *ret = *curr;
            *curr = (*curr)->next;
            return ret;
        } else {
            curr = &(*curr)->next;
        }
    }

    return NULL;
}

void *__wasm_memsafety_malloc(size_t align, size_t size) {
    align = align > 16 ? align : 16;
    // round up size to the next multiple of align
    // TODO: check if this works for non powers of two
    size_t aligned_size = (size + (align - 1)) & (~align - 1);
    void *mem = aligned_alloc(align, aligned_size);
    if (mem) {
        tagged_ptr_t tagged_ptr = __builtin_wasm_segment_new_stack(mem, size);
        Node *newHead = (Node *) malloc(sizeof(Node));
        newHead->next = head;
        newHead->original_ptr = mem;
        newHead->tagged_ptr = tagged_ptr;
        newHead->size = size;
        fprintf(stderr, "Tagging memory %p, size %zu\n", mem, size);
    }

    return mem;
}

void __wasm_memsafety_free(tagged_ptr_t ptr) {
    Node *node = __wasm_memsafety_find(&head, ptr);
    if (node) {
        __builtin_wasm_segment_free(node->tagged_ptr, node->size);
        fprintf(stderr, "Untagging memory %p, size %zu\n", node->original_ptr, node->size);
    }

    free(node->original_ptr);
    free(node);
}
