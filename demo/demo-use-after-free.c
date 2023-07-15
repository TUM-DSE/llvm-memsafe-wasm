#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc < 5) {
		printf("Usage: %s heapsize index value free_first\n", argv[0]);
		return 1;
	}

	int heapsize = atoi(argv[1]);
	int index = atoi(argv[2]);
	int value = atoi(argv[3]);
	int free_first = atoi(argv[4]);

	int volatile *x = malloc(heapsize * sizeof(int));

	x[index] = value;
	if (free_first) {
		free((void *) x);
	}
	printf("val = %d\n", x[index]);

	// avoid double free and memory-leak, though program should crash before this anyways if MTE works
	if (!free_first) {
		free((void *) x);
	}

	return 0;
}
