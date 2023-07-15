#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc < 6) {
		printf("Usage: %s [heapsize] [reallocsize] [index] [value] [free_first]\n", argv[0]);
		return 1;
	}

	int heapsize = atoi(argv[1]);
	int reallocsize = atoi(argv[2]);
	int index = atoi(argv[3]);
	int value = atoi(argv[4]);
	int free_first = atoi(argv[5]); // 1 or 0

	int *heap = (int*) calloc(sizeof(int), heapsize);
	if (heap == NULL) {
        printf("calloc returned NULL\n");
        return 1;
	}

	printf("heap[%d] = %d\n", index, value);
	heap[index] = value;

	printf("before realloc:\n");

	printf("val == %d\n", heap[index]);
	printf("heap[%d] == %d (should be 0 due to calloc)\n", heapsize-1, heap[heapsize-1]);

	heap = (int*) realloc((void*)heap, reallocsize * sizeof(int));
	if (heap == NULL) {
        printf("realloc returned NULL\n");
        return 1;
    }

	printf("after realloc:\n");

	// free the pointer (i.e. underlying memory) before using pointer
	heap[index] = value;
	if (free_first) {
		printf("Freeing pointer, even though it will be used later\n");
		free((void *) heap);
	}
	printf("val == %d\n", heap[index]);

	// avoid double free and memory-leak, though program should crash before this anyways if MTE works
	if (!free_first) {
		free((void *) heap);
	}

	return 0;
}
