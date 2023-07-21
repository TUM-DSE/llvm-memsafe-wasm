#include <stdio.h>
#include <stdlib.h>
#include "external.h"

void print_pointer_to_string(char **ptr) {
    printf("Printing pointer to string: %p\n", *ptr);
    // Signing *ptr should not be done because of this external function using it
    external_function(ptr);
}

__attribute__((noinline))
int main() {
    char *string = "Hello World!";

    // store pointer: we should insert a PAC sign here to prevent the attacker from overwriting it by overflowing `name`
    // if the attacker overflows `name`, then `pointer_storage` will be overwritten
    // char **pointer_storage = &string;
    char **pointer_storage;
    // read variable length user input into this array
	char name[10];

    // If we remove this line, then the loading of pointer storage below would also succeed
    // store_pointer(&string, &pointer_storage);
    pointer_storage = &string;

    // We should be able to pass aliases of the pointer_storage to other functions, as long as they don't end in external functions
    // print_pointer_to_string(pointer_storage);

	printf("What is your name?\n");
	scanf("%s", name);

	printf("Hello user %s!\n", name);

    // load pointer: we should insert a PAC auth here
    char *loaded_string = *pointer_storage;
    printf("Here is the string we stored and protected using PAC: %s\n", loaded_string);

	return 0;
}
