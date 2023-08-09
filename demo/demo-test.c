#include <stdio.h>
#include <stdlib.h>

int main() {
    char *string = "Hello World!";
    char **pointer_storage = &string;
    char name[10];

    printf("What is your name?\n");
    scanf("%s", name);  // potential buffer overflow
    printf("Hello user %s!\n", name);

    char *loaded_string = *pointer_storage;  // failed authentication
    printf("String protected with PAC: %s\n", loaded_string);

    return 0;
}
