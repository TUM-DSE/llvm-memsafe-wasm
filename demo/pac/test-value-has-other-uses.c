#include<stdlib.h>

// === TEST-CASE 1:

// this function is external
void external_function(int *ptr);

// our llvm should detect that ptr has other uses, since it is passed to an external function
void main() {
    int *ptr = get_pointer_from_not_elsewhere();
    external_function(ptr);
    return 0;
}

// Equivalent llvm-ir:
/*
*/

// === TEST-CASE 2:

// this function is external
void external_function(int *ptr);

// this function is not external (since it is defined), but it does call an external function
void non_external_function(int *ptr) {
    external_function(ptr);
}

// our llvm should detect that ptr has other uses, since it is indirectly passed to an external function
void main() {
    int *ptr = get_pointer_from_not_elsewhere();
    non_external_function(ptr);
    return 0;
}

// Equivalent llvm-ir:
/*
*/
