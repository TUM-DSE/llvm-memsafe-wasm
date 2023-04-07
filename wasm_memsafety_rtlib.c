#include <stdlib.h>
#include <stdio.h>

struct Node {
    struct Node *next;
    void *ptr;
    size_t size;
};

typedef struct Node Node;

static struct Node *head = NULL;


/// Find and remove a node in the list
struct Node *__wasm_memsafety_find(Node **head, void *ptr) {
    Node **curr = head;
    while (*curr) {
        if ((*curr)->ptr == ptr) {
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
    size_t aligned_size = (size + (align - 1)) & (~align);
    void *mem = aligned_alloc(align, aligned_size);
    if (mem) {
        Node *newHead = (Node *) malloc(sizeof(Node));
        newHead->next = head;
        newHead->ptr = mem;
        newHead->size = size;
        fprintf(stderr, "Tagging memory %p, size %zu\n", mem, size);
        // TODO: tag the memory here
    }

    return mem;
}

void __wasm_memsafety_free(void *ptr) {
    Node *node = __wasm_memsafety_find(&head, ptr);
    // TODO: untag the memory here
    fprintf(stderr, "Tagging memory %p, size %zu\n", ptr, node->size);

    free(ptr);
}
