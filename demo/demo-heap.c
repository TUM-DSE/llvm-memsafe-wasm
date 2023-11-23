#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: %s heapsize index value\n", argv[0]);
    return 1;
  }

  int heapsize = atoi(argv[1]);
  int index = atoi(argv[2]);
  int value = atoi(argv[3]);

  int volatile *x = malloc(heapsize * sizeof(int));

  x[index] = value;
  printf("val = %d\n", x[index]);

  free((void *)x);

  return 0;
}
