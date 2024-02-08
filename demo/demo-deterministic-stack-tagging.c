#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t get_mte_tag(void* address) {
    // Convert to 64-bit integer type.
    uint64_t address_as_int = (uint64_t)address;
    // Select only the 4-bit MTE tag.
    uint8_t tag = (address_as_int >> 56) & 0xF;

    return tag;
}

void test_heap() {
	// Heap arrays should be tagged randomly and independently (with IRG).
    char *heap_alloc_1 = (char*)malloc(sizeof(char) * 4);
    printf("heap alloc; heap_alloc_1; tag: %d; address %p; expected: random tag\n", get_mte_tag((void*)heap_alloc_1), heap_alloc_1);
	printf("Read something into heap array to prevent it from being optimized away.\n");
	scanf("%s", heap_alloc_1);

    char *heap_alloc_2 = (char*)malloc(sizeof(char) * 4);
    printf("heap alloc; heap_alloc_2; tag: %d; address %p; expected: random tag\n", get_mte_tag((void*)heap_alloc_2), heap_alloc_2);
	printf("Read something into heap array to prevent it from being optimized away.\n");
	scanf("%s", heap_alloc_2);

	printf("Printing heap arrays to prevent them from being optimized away: %s %s.\n", heap_alloc_1, heap_alloc_2);

    free(heap_alloc_1);
    free(heap_alloc_2);
}

void test_stack() {
	// Make multiple stack allocations, and ensure each is protected by MTE.
	// This is to test our deterministic tagging, since the first and the subsequent allocation in each C function are tagged differently.

	int num_stack_allocs;
	printf("How many stack allocations should be made?\n");
	scanf("%d", &num_stack_allocs);

	// Handle some cases manually (since a runtime-defined number of stack arrays can't be allocated at runtime).
	if (num_stack_allocs == 1) {
		printf("Allocating one stack array.\n");

		char name[10]; // who has a name longer than 9 chars anyway?
        printf("stack alloc; name[10]; tag: %d; address %p; expected tag: X\n", get_mte_tag((void*)name), name);

		printf("What is your name?\n");
		// Expect MTE fault on reading 16+ chars (one tag granule).
		scanf("%s", name);

		printf("Hello user %s!\n", name);
	} else if (num_stack_allocs == 2) {
		printf("Allocating two stack arrays.\n");

		char first_name[10];
        printf("stack alloc; first_name[10]; tag: %d; address %p; expected tag: X+1\n", get_mte_tag((void*)first_name), first_name);

		printf("What is your first name?\n");
		scanf("%s", first_name);

		char last_name[10];
        printf("stack alloc; last_name[10]; tag: %d; address %p; expected tag: X+2\n", get_mte_tag((void*)last_name), last_name);

		printf("What is your last name?\n");
		scanf("%s", last_name);

		printf("Hello user %s %s!\n", first_name, last_name);
	} else if (num_stack_allocs == 3) {
		printf("Allocating three stack arrays.\n");

		char first_name[10];
        printf("stack alloc; first_name[10]; tag: %d; address %p; expected tag: X+3\n", get_mte_tag((void*)first_name), first_name);

		printf("What is your first name?\n");
		scanf("%s", first_name);

		char second_name[10];
        printf("stack alloc; second_name[10]; tag: %d; address %p; expected tag: X+4\n", get_mte_tag((void*)second_name), second_name);

		printf("What is your second name?\n");
		scanf("%s", second_name);

		char third_name[10];
        printf("stack alloc; third_name[10]; tag: %d; address %p; expected tag: X+5\n", get_mte_tag((void*)third_name), third_name);

		printf("What is your third name?\n");
		scanf("%s", third_name);

		printf("Hello user %s %s %s!\n", first_name, second_name, third_name);
	} else {
		printf("%d stack allocations are not supported by this test.\n", num_stack_allocs);
	}
}

int main(int argc, char **argv) {
	test_heap();
	test_stack();

	return 0;
}
