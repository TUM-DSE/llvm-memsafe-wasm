#include <stdio.h>
#include <stdlib.h>
#include "demo-pac-external-functions.h"

void print_pointer_external(volatile int *ptr) {
  printf("Printing pointer: %d\n", *ptr);
}
