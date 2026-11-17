#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmp_int(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) printf("Usage: %s <N>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    long total_n = atol(argv[1]);
    long local_n = total_n / size;
    int *local_data = (int *)malloc(local_n * sizeof(int));

    srand(42 + rank * 101);
    for (long i = 0; i < local_n; i++) local_data[i] = rand();

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    qsort(local_data, local_n, sizeof(int), cmp_int);

    if (size == 1) {
        double elapsed = MPI_Wtime() - t_start;
        int valid = 1;
        for (long i = 1; i < local_n; i++) {
            if (local_data[i - 1] > local_data[i]) { valid = 0; break; }
        }
        printf("Parallel P=1 | N=%ld | MaxTime=%.6f s | Valid=%s\n", total_n, elapsed, valid ? "YES" : "NO");
        free(local_data);
        MPI_Finalize();
        return 0;
    }

    int num_samples = size - 1;
    int *local_samples = (int *)malloc(num_samples * sizeof(int));
    for (int i = 0; i < num_samples; i++) {
        local_samples[i] = local_data[(i + 1) * local_n / size];
    }

    int *all_samples = (rank == 0) ? (int *)malloc(size * num_samples * sizeof(int)) : NULL;
    MPI_Gather(local_samples, num_samples, MPI_INT, all_samples, num_samples, MPI_INT, 0, MPI_COMM_WORLD);

    int *splitters = (int *)malloc(num_samples * sizeof(int));
    if (rank == 0) {
        qsort(all_samples, size * num_samples, sizeof(int), cmp_int);
        for (int i = 0; i < num_samples; i++) splitters[i] = all_samples[(i + 1) * num_samples - 1];
        free(all_samples);
    }
    MPI_Bcast(splitters, num_samples, MPI_INT, 0, MPI_COMM_WORLD);

    int *send_counts = (int *)calloc(size, sizeof(int));
    int bucket = 0;
    for (long i = 0; i < local_n; i++) {
        while (bucket < num_samples && local_data[i] > splitters[bucket]) bucket++;
        send_counts[bucket]++;
    }

    int *send_displs = (int *)malloc(size * sizeof(int));
    send_displs[0] = 0;
    for (int i = 1; i < size; i++) send_displs[i] = send_displs[i - 1] + send_counts[i - 1];

    int *recv_counts = (int *)malloc(size * sizeof(int));
    MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, MPI_COMM_WORLD);

    int *recv_displs = (int *)malloc(size * sizeof(int));
    recv_displs[0] = 0;
    long total_recv = recv_counts[0];
    for (int i = 1; i < size; i++) {
        recv_displs[i] = recv_displs[i - 1] + recv_counts[i - 1];
        total_recv += recv_counts[i];
    }

    int *recv_data = (int *)malloc(total_recv * sizeof(int));
    MPI_Alltoallv(local_data, send_counts, send_displs, MPI_INT, recv_data, recv_counts, recv_displs, MPI_INT, MPI_COMM_WORLD);

    qsort(recv_data, total_recv, sizeof(int), cmp_int);

    double local_elapsed = MPI_Wtime() - t_start;
    double max_elapsed;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    int local_valid = 1;
    for (long i = 1; i < total_recv; i++) {
        if (recv_data[i - 1] > recv_data[i]) { local_valid = 0; break; }
    }
    int my_min = (total_recv > 0) ? recv_data[0] : 0;
    int my_max = (total_recv > 0) ? recv_data[total_recv - 1] : 0;
    int *all_mins = (rank == 0) ? (int *)malloc(size * sizeof(int)) : NULL;
    int *all_maxs = (rank == 0) ? (int *)malloc(size * sizeof(int)) : NULL;
    int *all_valids = (rank == 0) ? (int *)malloc(size * sizeof(int)) : NULL;
    MPI_Gather(&my_min, 1, MPI_INT, all_mins, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&my_max, 1, MPI_INT, all_maxs, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_valid, 1, MPI_INT, all_valids, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int global_valid = 1;
        for (int i = 0; i < size; i++) {
            if (!all_valids[i]) global_valid = 0;
            if (i > 0 && all_maxs[i - 1] > all_mins[i]) global_valid = 0;
        }
        printf("Parallel P=%d | N=%ld | MaxTime=%.6f s | Valid=%s\n", size, total_n, max_elapsed, global_valid ? "YES" : "NO");
        free(all_mins); free(all_maxs); free(all_valids);
    }

    free(local_data); free(local_samples); free(splitters);
    free(send_counts); free(send_displs); free(recv_counts); free(recv_displs); free(recv_data);
    MPI_Finalize();
    return 0;
}
