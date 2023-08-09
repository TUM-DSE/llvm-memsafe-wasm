int** sign_pointer() {
    int *ptr = (int*) malloc(20);
    int **memory_location = (int**) malloc(1);
    // store pointer
    *memory_location = ptr;
    return memory_location;
}

int main() {
    int **memory_location = sign_pointer();

    // load pointer
    int *loaded_ptr = *memory_location;
}
