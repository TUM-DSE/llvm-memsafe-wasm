#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    size_t n = 100000;

    void* ptrArray[n];

    // Store n pointers in the array
    for (size_t i = 0; i < n; i++) {
        ptrArray[i] = (void*) i; // casting the iterating variable to a pointer
    }

    // Load the n pointers from the array and accumulate their values
    size_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += (size_t) ptrArray[i];
    }

    return sum % 125;  // modulo to make sure it's a valid return code
}
