#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define NUM_CALLS 100000
#define REPEAT_COUNT 5

void* dummy_function(void *) { return NULL; }

double bench_threads()
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_t tid;
    for (int i = 0; i < NUM_CALLS; i++)
    {
        pthread_create(&tid, NULL, dummy_function, NULL);
        pthread_join(tid, NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long sec_diff = end.tv_sec - start.tv_sec;
    return (sec_diff * 1e9 + end.tv_nsec - start.tv_nsec) / NUM_CALLS;
}
int less_than(const void *a, const void *b)
{
    return (int)(*(double *)a - *(double *)b);
}
int main()
{
    printf("Benchmarking pthreads create + join\n");
    double benching_results[REPEAT_COUNT];
    for(int i = 0; i < REPEAT_COUNT; i++)
    {
        benching_results[i] = bench_threads();
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);

}