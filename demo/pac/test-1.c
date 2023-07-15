int *some_declared_function(int *ptr);

int *increment_ptr(int *ptr) {
    printf("print something just so this function doesn't get inlined, so the returned pointer can't be identified as an alias.");
    return ptr;
}

int main() {
    int buf[4];
    int x = 42;

    int *alias_to_buf_2 = increment_ptr(&buf[1]);
    some_declared_function(alias_to_buf_2);

    return 0;
}