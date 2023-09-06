#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <length_of_array>\n", argv[0]);
        return 1;
    }

    // size_t n = (size_t) atoi(argv[1]);
    size_t n = 40000;
    // void** arr = (void**)malloc(n * sizeof(void*));
    void* arr[n];

    // Read unsorted input array from stdin
    for (size_t i = 0; i < n; i++) {
        // int value;
        // scanf("%d", &value);
        // arr[i] = (void*)value;
        arr[i] = (void*)(n-i);
    }

    // We inline bubble sort here, so that our LLVM is able to insert PAC instructions
    for (size_t i = 0; i < n-1; i++) {
        for (size_t j = 0; j < n-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                void* temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }

    // Assert that the array was sorted correctly
    int assert_sorted = 1;
    for (size_t i = 0; i < n-1; i++) {
        if (arr[i] > arr[i+1]) {
            assert_sorted = 0;
            break;
        }
    }

    // We aren't allowed to free the array, as this counts as using the value elsewhere, so we memory-leak here for testing purposes
    // free(arr);

    if (!assert_sorted) {
        fprintf(stderr, "Array is not sorted!\n");
        return 1; // Error
    }

    return 0;
}
