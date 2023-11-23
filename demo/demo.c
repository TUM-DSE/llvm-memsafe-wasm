#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: %s index value\n", argv[0]);
    return 1;
  }
  volatile int x[16];
  int index = atoi(argv[1]);
  int value = atoi(argv[2]);
  x[index] = value;
  printf("val = %d\n", x[index]);
  return 0;
}
