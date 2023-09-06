#include <stdio.h>
#include <stdlib.h>

// TODO: use size_t over int

void bubble_sort(int* arr, int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                int temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
}

int assert_sorted(int* arr, int n) {
    for (int i = 0; i < n-1; i++) {
        if (arr[i] > arr[i+1]) {
            return 0; // Not sorted
        }
    }
    return 1; // Sorted
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <length_of_array>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int* arr = (int*)malloc(n * sizeof(int));

    // Read unsorted input array from stdin
    for (int i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }

    // Invoke SUT
    bubble_sort(arr, n);

    if (!assert_sorted(arr, n)) {
        fprintf(stderr, "Array is not sorted!\n");
        free(arr);
        return 1; // Error
    }

    free(arr);

    return 0;
}
