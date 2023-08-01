#include <stdio.h>

int main() {
    char **argv;

    while (argv++) {
        printf("%s\n" ,*argv);
    }

    return 0;
}