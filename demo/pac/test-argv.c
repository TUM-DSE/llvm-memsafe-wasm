#include <stdio.h>

int main(int argc, char **argv) {
    printf("argv[0] = %s\n", argv[0]);

    // // Internally, this looks like so:
    // // wasi_libc_get_argv() is an external function, meaning it doesn't sign the pointers it has stored
    // char **argv = wasi_libc_get_argv();
    // // now we load a non-signed pointer, so we are not allowed to auth it
    // argv[0];

    return 0;
}