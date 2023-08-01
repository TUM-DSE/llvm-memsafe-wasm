#include <stdio.h>
#include <stdlib.h>
#include "demo-pac-external-functions.h"

// void store_pointer(int *value_ptr, int **dst_ptr) {
//   *dst_ptr = value_ptr;
// }

// int *load_pointer(int **x) {
//   return *x;
// }

// int main(int argc, char **argv) {
// 	if (argc < 2) {
// 		printf("Usage: %s value\n", argv[0]);
// 		return 1;
// 	}

//   int *buf[4];
// 	int x = atoi(argv[1]);

//   // &buf[1] is signed with PAC here
//   store_pointer(&x, &buf[1]);

//   // load succeeds because &buf[1] can be authenticated with PAC
//   int *x_ptr = load_pointer(&buf[1]);

//   printf("Value stored in x is: %d\n", *x_ptr);

//   return 0;
// }


// void print_pointer(int *ptr) {
//   printf("Printing pointer: %d\n", *ptr);
// }

// void print_double_pointer(int **ptr) {
//   printf("Printing pointer: %d\n", **ptr);
// }

// int main(int argc, char **argv) {
// 	if (argc < 2) {
// 		printf("Usage: %s value\n", argv[0]);
// 		return 1;
// 	}

//   int *buf[4];
// 	int x = atoi(argv[1]);
//   int *x_pointer = &x;

//   // &x is signed with PAC here
//   buf[1] = &x;

//   // These should both be recognized by as users of x
//   print_pointer(&x);
//   print_double_pointer(&x_pointer);

//   // load succeeds because &x can be authenticated with PAC
//   int *x_ptr = *(&buf[1]);

//   printf("Value stored in x is: %d\n", *x_ptr);

//   return 0;
// }


// __attribute__((noinline))
// void print_pointer(volatile int *ptr) {
//   printf("Printing pointer: %d\n", *ptr);
// }

// __attribute__((noinline))
// int main(int argc, char **argv) {
// 	if (argc < 2) {
// 		printf("Usage: %s value\n", argv[0]);
// 		return 1;
// 	}

//   volatile int *buf[4];
// 	volatile int x = atoi(argv[1]);

//   // &x is signed with PAC here
//   // this would count as alias already
//   buf[2] = &x;

//   // These should both be recognized by us as users of x
//   print_pointer(&x);
//   print_pointer(buf[2]);

//   // load succeeds because &x can be authenticated with PAC
//   volatile int *x_ptr = *(&buf[2]);

//   printf("Value stored in x is: %d\n", *x_ptr);

//   return 0;
// }


// int main(int argc, char **argv) {
//   int return_val = (int) argv[0];
//   printf("Code until here was executed.\n");
//   return return_val;
// }


void print_pointer_external(volatile int *ptr);

__attribute__((noinline))
void print_pointer(volatile int *ptr) {
  printf("Printing pointer: %d\n", *ptr);
}

// expect no auths or signs to be inserted, since they alias each other
__attribute__((noinline))
int main(int argc, char **argv) {
  // this should be alias of y
  volatile int x = 42;
  printf("x before: %d\n", x);

  // this should be alias of x
  volatile int *y = &x;
  *y = 41;
  printf("x after: %d\n", x);

  // for now, this should be disallowed
  // print_pointer(y);
  // print_pointer_external(y);
  // print_pointer(&x);
  print_pointer_external(&x);

  return 0;
}
