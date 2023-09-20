#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s integer\n", argv[0]);
		return 1;
	}

	int integer = atoi(argv[1]);
    printf("integer = %d\n", integer);

    int stack_alloc_1[10];
    printf("expect random tag (first stack) X; stack_alloc_1 = %p\n", stack_alloc_1);

    int x = integer + 42;

    int stack_alloc_2[20];
    printf("expect incremented tag X += 1; stack_alloc_2 = %p\n", stack_alloc_2);

    int *heap_alloc_1 = (int*)malloc(sizeof(int) * 4);
    printf("expect random tag (heap) Y; heap_alloc2 = %p\n", heap_alloc_1);

    int stack_alloc_3[20];
    printf("expect incremented tag X += 1; stack_alloc_3 = %p\n", stack_alloc_3);

    // Do something with the arrays, to prevent being optimized away
    stack_alloc_1[0] = integer;
    stack_alloc_2[0] = integer;
    stack_alloc_3[0] = integer;
    heap_alloc_1[0] = integer;

    free(heap_alloc_1);

    return 0;
}