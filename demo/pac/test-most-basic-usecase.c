#include <stdio.h>
#include <stdlib.h>

// // This store would never get signed, because the memory location the pointer is stored to comes from the parameter of the function.
// void store_pointer(int *value_ptr, int **dst_ptr) {
//     // store pointer
//     *dst_ptr = value_ptr;
// }

// // This load would never get signed, because the memory location the pointer is loaded from comes from the parameter of the function.
// int *load_pointer(int **x) {
//     // load pointer
//     return *x;
// }

// int main(int argc, char **argv) {
//     if (argc < 2) {
//         printf("Usage: %s value\n", argv[0]);
//         return 1;
//     }

//     // int *buf[4];
//     int **buf;
//     int x = atoi(argv[1]);

//     // &x should be signed with PAC here before storing
//     // store pointer
//     // buf[1] = &x;

//     // no signing, see explanation in method
//     // store_pointer(&x, &buf[1]);
//     store_pointer(&x, buf);

//     // &x should be authenticated with PAC here after loading
//     // load pointer
//     // int *x_ptr = buf[1];
//     int *x_ptr = *buf;

//     // no authentication, see explanation in method
//     // int *x_ptr = load_pointer(&buf[1]);

//     printf("Value stored in x is: %d\n", *x_ptr);

//     return 0;
// }

void store_pointer(int *value_ptr, int **dst_ptr) {
    // store pointer
    *dst_ptr = value_ptr;
}

int *load_pointer(int **x) {
    return *x;
}

int main() {
    int **buf;
    int x = 42;

    buf[1] = &x;

    int *x_ptr = load_pointer(&buf[1]);

    int x_alias = *x_ptr;

    return 0;
}
