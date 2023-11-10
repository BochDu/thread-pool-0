#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// task struct
typedef struct
{
    void (*fun)(void *arg);
    void *arg;
} task_t;

// thread pool struct
typedef struct
{
    // task queue
    task_t *task_queue;
    int queue_capacity; // total task num
    int queue_size;     // current task num
    int queue_front;    // head -> get data
    int queue_rear;     // tail -> set data

    pthread_t manage_id;  // manage thread id
    pthread_t *thread_id; // work thread id
    int thread_min;       // min work thread num
    int thread_max;       // max work thread num
    int thread_busy;      // busy work thread num
    int thread_alive;     // alive work thread num
    int thread_exit;      // destroy thread num

    pthread_mutex_t mutex_pool; // lock whole pool
    pthread_mutex_t mutex_busy; // lock busy num

    pthread_cond_t notfull;  // task queue filled
    pthread_cond_t notempty; // task queue empty

    int shutdown; // destroy pool
} thread_pool_t;

thread_pool_t *thread_pool_create(int thread_min, int thread_max, int queue_capacity);
int thread_pool_destroy(thread_pool_t *pool);

void *worker_routine(void *arg);
void *manager_routine(void *arg);
void thread_pool_add_task(thread_pool_t *pool, void (*fun)(void *), void *arg);

int thread_pool_get_busy(thread_pool_t *pool);
int thread_pool_get_alive(thread_pool_t *pool);
#endif