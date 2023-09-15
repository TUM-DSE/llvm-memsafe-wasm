#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 5) {
		printf("Usage: %s [array size] [write index] [write content] [read index]\n", argv[0]);
		return 1;
	}

	int size = atoi(argv[1]);
	int write_index = atoi(argv[2]);
	int write_content = atoi(argv[3]);
	int read_index = atoi(argv[4]);

    int *arr = (int *) malloc(sizeof(int) * size);

	arr[write_index] = write_content;
	printf("arr[%d] = %d\n", write_index, arr[read_index]);

    free(arr);

	return 0;
}
