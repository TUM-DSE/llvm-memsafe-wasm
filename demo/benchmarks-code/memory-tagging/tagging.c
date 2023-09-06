// #include <stdio.h>
// #include <stdlib.h>

// int main() {
//     size_t n = 1000;
//     // volatile int static_size_array[1000];
//     volatile int static_size_array[n];

//     // for (int i = 0; i < n; i++) {
//     //     static_size_array[i] = i % 255;
//     // }

//     for (int i = 0; i < n; i++) {
//         static_size_array[i] = i % 255;
//     }

//     // Pretend to use the data
//     use_array(static_size_array, n);

//     return 0;
// }


#include <stdio.h>
#include <stdlib.h>

int check = 0; // global variable

int main() {
    size_t n = 10000;
    size_t alignment = 32;
    int static_size_array[n * 32];
    // int *static_size_array = malloc(sizeof(int) * (n*32));

    for (int i = 0; i < n; i++) {
        static_size_array[i] = i % 255;
    }

    // Unpredictable branch to compiler, will never actually run, 
    // but compiler doesn't know that for sure
    if (check) {
        printf("%d", static_size_array[0]);
    }

    return 0;
}
