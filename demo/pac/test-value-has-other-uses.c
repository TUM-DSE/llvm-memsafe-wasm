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

// === TEST-CASE 3:

// this function is external
void external_function(int *ptr);

// this function is not external (since it is defined), but it does call an external function for one of its parameters
void non_external_function(int *ptr1, int *ptr2) {
    // ptr1 is unused;
    external_function(ptr2);
}

// our llvm should detect that ptr has other uses, since it is indirectly passed to an external function
void main() {
    int *ptr = get_pointer_from_not_elsewhere();
    non_external_function(ptr, ptr);
    return 0;
}

// Equivalent llvm-ir:
/*
*/
