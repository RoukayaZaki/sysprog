#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>

#define NUM_CALLS 50000000
#define REPEAT_COUNT 5

double bench_clock(clockid_t clock_type)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_CALLS; i++)
    {
        clock_gettime(clock_type, NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long sec_diff = end.tv_sec - start.tv_sec;
    // printf("Sec diff: %ld, Start: %'ld, End: %'ld\n", sec_diff, start.tv_nsec, end.tv_nsec);
    return (sec_diff * 1e9 + end.tv_nsec - start.tv_nsec) / NUM_CALLS;
}

int less_than(const void *a, const void *b)
{
    return (int)(*(double *)a - *(double *)b);
}

int main()
{
    // Task 1

    printf("Benchmarking 'clock_gettime(CLOCK_REALTIME)' (%d runs)\n", REPEAT_COUNT);
    double benching_results[REPEAT_COUNT];
    for (int i = 0; i < REPEAT_COUNT; i++)
    {
        benching_results[i] = bench_clock(CLOCK_REALTIME);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);
    printf("\n");

    printf("Benchmarking 'clock_gettime(CLOCK_MONOTONIC)' (%d runs)\n", REPEAT_COUNT);
    for (int i = 0; i < REPEAT_COUNT; i++)
    {
        benching_results[i] = bench_clock(CLOCK_MONOTONIC);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);
    printf("\n");

    printf("Benchmarking 'clock_gettime(CLOCK_MONOTONIC_RAW)' (%d runs)\n", REPEAT_COUNT);
    for (int i = 0; i < REPEAT_COUNT; i++)
    {
        benching_results[i] = bench_clock(CLOCK_MONOTONIC_RAW);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);

    return 0;
}
