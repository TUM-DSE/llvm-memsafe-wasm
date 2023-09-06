#include <stdio.h>
#include <stdlib.h>

#define LIMIT 160

void perform_merge(int *arr, const int *L, const int *R, size_t l, size_t n1, size_t n2) {
    size_t i = 0, j = 0, k = l;

    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) {
        arr[k++] = L[i++];
    }

    while (j < n2) {
        arr[k++] = R[j++];
    }
}

void merge(int* arr, size_t l, size_t m, size_t r) {
    size_t L_size = m - l + 1;
    size_t R_size = r - m;

    // For small paritition sizes, allocate constant size on the stack to take advantage of our optimizations
    if (L_size <= LIMIT && R_size <= LIMIT) {
        int L[LIMIT], R[LIMIT];

        // Copy data to temp arrays L[] and R[]
        for (size_t i = 0; i < L_size; i++)
            L[i] = arr[l + i];
        for (size_t j = 0; j < R_size; j++)
            R[j] = arr[m + 1 + j];

        perform_merge(arr, L, R, l, L_size, R_size);
    } else {
        int *L = (int *) malloc(L_size * sizeof(int));
        int *R = (int *) malloc(R_size * sizeof(int));

        // Copy data to temp arrays L[] and R[]
        for (size_t i = 0; i < L_size; i++)
            L[i] = arr[l + i];
        for (size_t j = 0; j < R_size; j++)
            R[j] = arr[m + 1 + j];

        perform_merge(arr, L, R, l, L_size, R_size);

        free(L);
        free(R);
    }
}

void modified_merge_sort(int* arr, size_t l, size_t r) {
    if (l < r) {
        size_t m = l + (r - l) / 2;
        modified_merge_sort(arr, l, m);
        modified_merge_sort(arr, m + 1, r);
        merge(arr, l, m, r);
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
    modified_merge_sort(arr, 0, n - 1);

    if (!assert_sorted(arr, n)) {
        fprintf(stderr, "Array is not sorted!\n");
        free(arr);
        return 1; // Error
    }

    free(arr);

    return 0;
}

