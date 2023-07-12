#include <stdio.h>
#include <stdlib.h>

// void store_pointer(int *value_ptr, int **dst_ptr) {
//   *dst_ptr = value_ptr;
// }

// int *load_pointer(int **x) {
//   return *x;
// }

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s value\n", argv[0]);
        return 1;
    }

    int *buf[4];
    int x = atoi(argv[1]);

    // store pointer
    // &x should be signed with PAC here before storing
    buf[1] = &x;
    // store_pointer(&x, &buf[1]);

    // load pointer
    // &x should be authenticated with PAC here after loading
    int *x_ptr = buf[1];
    // int *x_ptr = load_pointer(&buf[1]);

    printf("Value stored in x is: %d\n", *x_ptr);

    return 0;
}

// int main() {
//     int *buf[4];
//     int x = 42;

//     int *x_ptr = &x;

//     // store pointer
//     buf[1] = x_ptr;

//     external_function(x_ptr);

//     return 0;
// }
