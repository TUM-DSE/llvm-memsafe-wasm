#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MyStruct {
    char *string;
    char **pointer_storage;
    char name[10];
};

void print_hello(struct MyStruct *s) {
    printf("Hello user %s!\n", s->name);
    char *loaded_string = *s->pointer_storage;
    printf("Here is the string we stored and protected using PAC: %s\n", loaded_string);
}

int main() {
    struct MyStruct s1 = {.string = "Hello World!", .pointer_storage = &s1.string};
    struct MyStruct s2 = {.string = "Hello again!", .pointer_storage = &s2.string};
    
    int struct_id;
    printf("Enter struct id (1 or 2):\n");
    scanf("%d", &struct_id);
    
    if (struct_id == 1) {
        printf("Enter your name:\n");
        scanf("%s", s1.name);
        print_hello(&s1);
    } else if (struct_id == 2) {
        printf("Enter your name:\n");
        scanf("%s", s2.name);
        print_hello(&s2);
    } else {
        printf("Invalid id.\n");
    }

    return 0;
}
