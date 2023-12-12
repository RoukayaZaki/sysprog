#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#define REPEAT_COUNT 5

#define MAX_VALUE 10000000

int counter = 0;
pthread_mutex_t lock; 

void *increment_function(void *order) {
    while(counter < MAX_VALUE)
    {
        pthread_mutex_lock(&lock); 
        counter++;
        pthread_mutex_unlock(&lock); 
    }
    return NULL;
}

double bench_threads(int num_threads)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    pthread_t tid[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        pthread_create(&tid[i], NULL, increment_function, NULL);
    }
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(tid[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long sec_diff = end.tv_sec - start.tv_sec;
    return (sec_diff * 1e9 + end.tv_nsec - start.tv_nsec) / MAX_VALUE;
}
int less_than(const void *a, const void *b)
{
    return (int)(*(double *)a - *(double *)b);
}
int main()
{
    printf("Benchmarking Pthread mutex lock\n");
    int num_threads;
    scanf("%d", &num_threads);
    printf("10 mln locks with %d threads\n", num_threads);
    double benching_results[REPEAT_COUNT];
    for(int i = 0; i < REPEAT_COUNT; i++)
    {
        counter = 0;
        benching_results[i] = bench_threads(num_threads);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);
    printf("\n");

    scanf("%d", &num_threads);
    printf("10 mln locks with %d threads\n", num_threads);
    for(int i = 0; i < REPEAT_COUNT; i++)
    {
        counter = 0;
        benching_results[i] = bench_threads(num_threads);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);
    printf("\n");

    scanf("%d", &num_threads);
    printf("10 mln locks with %d threads\n", num_threads);
    for(int i = 0; i < REPEAT_COUNT; i++)
    {
        counter = 0;
        benching_results[i] = bench_threads(num_threads);
    }
    qsort(benching_results, REPEAT_COUNT, sizeof(double), less_than);
    printf("\tMin: %.2f ns; Max: %.2f ns; Median: %.2f ns\n", benching_results[0], benching_results[REPEAT_COUNT - 1], benching_results[REPEAT_COUNT / 2]);
    printf("\n");
    pthread_mutex_destroy(&lock); 

}