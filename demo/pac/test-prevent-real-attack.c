#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline))
int main() {
    char *string = "Hello World!";

    // store pointer: we should insert a PAC sign here to prevent the attacker from overwriting it by overflowing `name`
    // if the attacker overflows `name`, then `pointer_storage` will be overwritten
    char **pointer_storage = &string;
    // read variable length user input into this array
	char name[10];

	printf("What is your name?\n");
	scanf("%s", name);

	printf("Hello user %s!\n", name);

    // load pointer: we should insert a PAC auth here
    char *loaded_string = *pointer_storage;
    printf("Here is the string we stored and protected using PAC: %s\n", loaded_string);

	return 0;
}
