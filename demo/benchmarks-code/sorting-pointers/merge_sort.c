#include <stdio.h>
#include <stdlib.h>

void merge(void** arr, size_t l, size_t m, size_t r) {
    size_t L_size = m - l + 1;
    size_t R_size = r - m;

    // create temporary arrays on the heap
    void** L = (void**)malloc(L_size * sizeof(void*));
    void** R = (void**)malloc(R_size * sizeof(void*));

    if (!L || !R) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // Copy data to temp arrays L[] and R[]
    for (size_t i = 0; i < L_size; i++)
        L[i] = arr[l + i];
    for (size_t j = 0; j < R_size; j++)
        R[j] = arr[m + 1 + j];

    // Merge the temp arrays back into arr[l..r]
    size_t i = 0;
    size_t j = 0;
    size_t k = l;
    while (i < L_size && j < R_size) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    // Copy the remaining elements of L[], if there are any
    while (i < L_size) {
        arr[k] = L[i];
        i++;
        k++;
    }

    // Copy the remaining elements of R[], if there are any
    while (j < R_size) {
        arr[k] = R[j];
        j++;
        k++;
    }

    free(L);
    free(R);
}

void merge_sort(void** arr, size_t l, size_t r) {
    if (l < r) {
        size_t m = l + (r - l) / 2;
        merge_sort(arr, l, m);
        merge_sort(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}

int assert_sorted(void** arr, size_t n) {
    for (size_t i = 0; i < n-1; i++) {
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

    size_t n = (size_t) atoi(argv[1]);
    void** arr = (void**)malloc(n * sizeof(void*));

    // Read unsorted input array from stdin
    for (size_t i = 0; i < n; i++) {
        int value;
        scanf("%d", &value);
        arr[i] = (void*)value;
    }

    // Invoke SUT
    merge_sort(arr, 0, n - 1);

    if (!assert_sorted(arr, n)) {
        fprintf(stderr, "Array is not sorted!\n");
        free(arr);
        return 1; // Error
    }

    free(arr);

    return 0;
}
