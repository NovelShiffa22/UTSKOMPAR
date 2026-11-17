#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int cmp_int(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <N>\n", argv[0]);
        return 1;
    }
    long n = atol(argv[1]);
    int *arr = (int *)malloc(n * sizeof(int));
    srand(42);
    for (long i = 0; i < n; i++) arr[i] = rand();

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    qsort(arr, n, sizeof(int), cmp_int);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    int valid = 1;
    for (long i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) { valid = 0; break; }
    }

    printf("Serial N=%ld | Time=%.6f s | Valid=%s\n", n, elapsed, valid ? "YES" : "NO");
    free(arr);
    return 0;
}
