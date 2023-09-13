#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s integer\n", argv[0]);
		return 1;
	}
	int integer = atoi(argv[1]);

    printf("integer = %d\n", integer);

    printf("expect random tag (first stack) X\n");
    int stack_alloc_1[10];
    int x = integer + 42;
    printf("expect incremented tag X += 1\n");
    int stack_alloc_2[20];
    printf("expect random tag (heap) Y\n");
    int *heap_alloc_1 = (int*)malloc(sizeof(int) * 4);
    printf("expect incremented tag X += 1\n");
    int stack_alloc_3[20];

    stack_alloc_1[0] = integer;
    stack_alloc_2[0] = integer;
    stack_alloc_3[0] = integer;
    heap_alloc_1[0] = integer;

    return 0;
}