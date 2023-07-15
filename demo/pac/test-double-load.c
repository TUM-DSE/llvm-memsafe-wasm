#include <stdio.h>

int main(int argc, char **argv) {
    // store argv
    char ***argv_ptr = &argv[0];

    // load argv
    printf("argv[0] = %s\n", *argv_ptr);

    return 0;
}
