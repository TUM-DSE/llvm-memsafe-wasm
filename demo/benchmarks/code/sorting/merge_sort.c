#include <stdio.h>
#include <stdlib.h>

// TODO: use size_t over int

void merge(int* arr, int l, int m, int r) {
    int L_size = m - l + 1;
    int R_size = r - m;

    // create temporary arrays on the heap
    int* L = (int*)malloc(L_size * sizeof(int));
    int* R = (int*)malloc(R_size * sizeof(int));

    if (!L || !R) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // Copy data to temp arrays L[] and R[]
    for (int i = 0; i < L_size; i++)
        L[i] = arr[l + i];
    for (int j = 0; j < R_size; j++)
        R[j] = arr[m + 1 + j];

    // Merge the temp arrays back into arr[l..r]
    int i = 0;
    int j = 0;
    int k = l;
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

void merge_sort(int* arr, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        merge_sort(arr, l, m);
        merge_sort(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}

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
    merge_sort(arr, 0, n - 1);

    if (!assert_sorted(arr, n)) {
        fprintf(stderr, "Array is not sorted!\n");
        free(arr);
        return 1; // Error
    }

    free(arr);

    return 0;
}
