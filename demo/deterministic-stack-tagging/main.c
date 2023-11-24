#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t get_mte_tag(int* address) {
    // Convert to 64-bit integer type.
    uint64_t address_as_int = (uint64_t)address;
    // Select only the 4-bit MTE tag.
    uint8_t tag = (address_as_int >> 56) & 0xF;

    return tag;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s integer\n", argv[0]);
		return 1;
	}

	int integer = atoi(argv[1]);
    printf("input integer (just printed so code below doesn't get optimized away) = %d\n", integer);

    int stack_alloc_1[10];
    printf("stack_alloc_1; tag: %d; address %p; expected random tag X (first stack array in function)\n", get_mte_tag(stack_alloc_1), stack_alloc_1);

    int x = integer + 42;

    int stack_alloc_2[20];
    printf("stack_alloc_2; tag: %d; address %p; expected incremented tag X+1\n", get_mte_tag(stack_alloc_2), stack_alloc_2);

    int *heap_alloc_1 = (int*)malloc(sizeof(int) * 4);
    printf("heap_alloc_1; tag: %d; address %p; expected random tag Y (first heap array in function)\n", get_mte_tag(heap_alloc_1), heap_alloc_1);

    int *heap_alloc_2 = (int*)malloc(sizeof(int) * 8);
    printf("heap_alloc_2; tag: %d; address %p; expected random tag Y+1\n", get_mte_tag(heap_alloc_2), heap_alloc_2);

    int stack_alloc_3[20];
    printf("stack_alloc_3; tag: %d; address %p; expected incremented tag X+2\n", get_mte_tag(stack_alloc_3), stack_alloc_3);

    int *heap_alloc_3 = (int*)malloc(sizeof(int) * 16);
    printf("heap_alloc_3; tag: %d; address %p; expected random tag Y+2\n", get_mte_tag(heap_alloc_3), heap_alloc_3);

    int stack_alloc_4[15];
    printf("stack_alloc_4; tag: %d; address %p; expected incremented tag X+3\n", get_mte_tag(stack_alloc_4), stack_alloc_4);

    // Do something with the arrays, to prevent being optimized away
    stack_alloc_1[0] = integer;
    stack_alloc_2[0] = integer;
    stack_alloc_3[0] = integer;
    stack_alloc_4[0] = integer;
    heap_alloc_1[0] = integer;
    heap_alloc_2[0] = integer;
    heap_alloc_3[0] = integer;

    free(heap_alloc_1);
    free(heap_alloc_2);
    free(heap_alloc_3);

    return 0;
}
