#include "thread_pool.h"
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0

struct thread_task
{
    thread_task_f function;
    void *arg;

    void *result;
    int status;
    pthread_mutex_t state_lock;
    pthread_cond_t is_finished;
    struct thread_pool *pool;
    struct thread_task *next;
};

struct thread_pool
{
    pthread_t *threads;

    int max_count;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t await_task;
    struct thread_task *upcoming_tasks;
    struct thread_task *last_tasks;
    int task_count;
    bool is_shutdown;
};

int thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if (max_thread_count > 0 && max_thread_count <= TPOOL_MAX_THREADS)
    {

        *pool = malloc(sizeof(struct thread_pool));
        if (*pool == NULL)
        {
            perror("No memory to allocate for thread pool");
            exit(1);
        }

        (*pool)->threads = NULL;
        (*pool)->max_count = max_thread_count;
        (*pool)->count = 0;

        pthread_mutex_init(&(*pool)->lock, NULL);
        pthread_cond_init(&(*pool)->await_task, NULL);
        (*pool)->upcoming_tasks = NULL;
        (*pool)->last_tasks = NULL;
        (*pool)->task_count = 0;

        (*pool)->is_shutdown = false;
        
        return SUCCESS;
    }
    
    return TPOOL_ERR_INVALID_ARGUMENT;
}

int thread_pool_thread_count(const struct thread_pool *pool)
{
    return pool->count;
}

int thread_pool_delete(struct thread_pool *pool)
{
    if (pool->task_count > 0)
    {
        return TPOOL_ERR_HAS_TASKS;
    }

    pthread_mutex_lock(&pool->lock);
    pool->is_shutdown = true;
    pthread_cond_broadcast(&pool->await_task);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->await_task);
    free(pool->threads);
    free(pool);

    return SUCCESS;
}

void *_thread_pool_worker(void *_pool)
{
    struct thread_pool *pool = _pool;

    for (;;)
    {
        pthread_mutex_lock(&pool->lock);
        struct thread_task *task = pool->upcoming_tasks;
        if (task != NULL)
        {

            pool->upcoming_tasks = task->next;
            if (pool->last_tasks == task)
            {
                pool->last_tasks = NULL;
            }

            task->next = NULL;
            pthread_mutex_unlock(&pool->lock);

            pthread_mutex_lock(&task->state_lock);
            task->status = 1;
            pthread_mutex_unlock(&task->state_lock);

            task->result = task->function(task->arg);

            pthread_mutex_lock(&task->state_lock);
            task->status = 2;
            pthread_cond_signal(&task->is_finished);
            pthread_mutex_unlock(&task->state_lock);
        }
        else
        {
            if (pool->is_shutdown)
            {
                pthread_mutex_unlock(&pool->lock);
                break;
            }
            else
            {
                pthread_cond_wait(&pool->await_task, &pool->lock);
                pthread_mutex_unlock(&pool->lock);
            }
        }
    }

    return NULL;
}

void _thread_pool_start_thread(struct thread_pool *pool)
{
    if (pool->count != pool->max_count)
    {
        pthread_t *new_threads =
            realloc(pool->threads, (pool->count + 1) * sizeof(pthread_t));
        if (new_threads != NULL)
        {
            pool->threads = new_threads;

            if (!(pthread_create(&pool->threads[pool->count], NULL,
                                 _thread_pool_worker, pool) == 0))
                return;
            pool->count++;
        }
    }
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    if (task->pool != NULL && task->status != 2)
    {
        return TPOOL_ERR_TASK_IN_POOL;
    }

    if (pool->task_count == TPOOL_MAX_TASKS)
    {
        return TPOOL_ERR_TOO_MANY_TASKS;
    }

    pthread_mutex_lock(&pool->lock);
    pool->task_count++;
    int task_count = pool->task_count;
    pthread_mutex_unlock(&pool->lock);

    task->pool = pool;
    task->status = 0;

    pthread_mutex_lock(&pool->lock);
    struct thread_task *previous = pool->last_tasks;
    pool->last_tasks = task;
    if (previous != NULL)
    {
        previous->next = task;
    }
    if (pool->upcoming_tasks == NULL)
    {
        pool->upcoming_tasks = task;
    }

    if (task_count > pool->count)
    {
        _thread_pool_start_thread(pool);
    }
    pthread_cond_signal(&pool->await_task);
    pthread_mutex_unlock(&pool->lock);

    return SUCCESS;
}

int thread_task_new(struct thread_task **task, thread_task_f function,
                    void *arg)
{
    *task = malloc(sizeof(struct thread_task));
    if (*task == NULL)
    {
        perror("No memory to allocate for thread task");
        exit(1);
    }

    (*task)->function = function;
    (*task)->arg = arg;

    (*task)->status = 0;
    pthread_mutex_init(&(*task)->state_lock, NULL);
    pthread_cond_init(&(*task)->is_finished, NULL);
    
    (*task)->pool = NULL;
    (*task)->next = NULL;

    return SUCCESS;
}

bool thread_task_is_finished(const struct thread_task *task)
{
    return task->status == 2;
}

bool thread_task_is_running(const struct thread_task *task)
{
    return task->status == 1;
}

int thread_task_join(struct thread_task *task, void **result)
{
    if (!(task->pool == NULL))
    {

        pthread_mutex_lock(&task->state_lock);
        for (;;)
        {
            if (task->status != 2)
            {
                pthread_cond_wait(&task->is_finished, &task->state_lock);
                continue;
            }
            break;
        }
        pthread_mutex_unlock(&task->state_lock);

        pthread_mutex_lock(&task->pool->lock);
        task->pool->task_count--;
        pthread_mutex_unlock(&task->pool->lock);

        task->pool = NULL;

        *result = (result == NULL)? NULL : task->result;
        return SUCCESS;
    }
    else
    {
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }
}

#ifdef NEED_TIMED_JOIN

int thread_task_timed_join(struct thread_task *task, double timeout,
                           void **result)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    (void)timeout;
    (void)result;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int thread_task_delete(struct thread_task *task)
{
    if (task->pool == NULL)
    {
        pthread_mutex_destroy(&task->state_lock);
        pthread_cond_destroy(&task->is_finished);
        free(task);
        return SUCCESS;
    }
    else
    {
        return TPOOL_ERR_TASK_IN_POOL;
    }
}

#ifdef NEED_DETACH

int thread_task_detach(struct thread_task *task)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
