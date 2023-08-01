#include <stdio.h>
#include "external.h"

void external_function(char **ptr) {
    //... (implementation of the function)
    printf("Printing from external function, ptr is: %p\n", *ptr);
}
