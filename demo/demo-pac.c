#include <stdio.h>
#include <stdlib.h>

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



void print_pointer(int *ptr) {
  printf("Printing pointer: %d\n", *ptr);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s value\n", argv[0]);
		return 1;
	}

  int *buf[4];
	int x = atoi(argv[1]);

  // &x is signed with PAC here
  buf[1] = &x;

  // These should both be recognized by as users of x
  print_pointer(&x);
  print_pointer(buf[1]);

  // load succeeds because &x can be authenticated with PAC
  int *x_ptr = *(&buf[1]);

  printf("Value stored in x is: %d\n", *x_ptr);

  return 0;
}
