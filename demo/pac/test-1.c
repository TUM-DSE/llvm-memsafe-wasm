// int *some_declared_function(int *ptr);

// int *increment_ptr(int *ptr) {
//     printf("print something just so this function doesn't get inlined, so the returned pointer can't be identified as an alias.");
//     return ptr;
// }

// int main() {
//     int buf[4];
//     int x = 42;

//     int *alias_to_buf_2 = increment_ptr(&buf[1]);
//     some_declared_function(alias_to_buf_2);

//     return 0;
// }


// this function is external
void external_function(int **ptr) {
    // load ptr and do sth
}

// this function is not external (since it is defined), but it does call an external function
void non_external_function(int **ptr) {
    external_function(ptr);
}

// our llvm should detect that ptr has other uses, since it is indirectly passed to an external function
int main() {
    int *buf[4];

    int x = 16;
    buf[1] = &x;
    non_external_function(&buf[1]);
    return 0;
}
