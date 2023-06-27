#include <stdio.h>
#include <stdlib.h>

void store_pointer(int *value_ptr, int **dst_ptr) {
  *dst_ptr = value_ptr;
}

int *load_pointer(int **x) {
  return *x;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s value\n", argv[0]);
		return 1;
	}

  int *buf[4];
	int x = atoi(argv[1]);

  // &buf[1] is signed with PAC here
  store_pointer(&x, &buf[1]);

  // load succeeds because &buf[1] can be authenticated with PAC
  int *x_ptr = load_pointer(&buf[1]);

  printf("Value stored in x is: %d\n", *x_ptr);

  return 0;
}

// void store_pointer(volatile int *value_ptr, volatile int **dst_ptr) {
//   *dst_ptr = value_ptr;
// }

// volatile int *load_pointer(volatile int **x) {
//   return *x;
// }

// int main(int argc, char **argv) {

//   volatile int *buf[4];
// 	volatile int x = 42;

//   // &buf[1] is signed with PAC here
//   store_pointer(&x, &buf[1]);

//   // load succeeds because &buf[1] can be authenticated with PAC
//   volatile int *x_ptr = load_pointer(&buf[1]);

//   printf("Value stored in x is: %d", *x_ptr);

//   return 0;
// }
