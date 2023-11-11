#include "thread_pool.h"

#define THREAD_ADD_NUM 2
#define THREAD_DESTROY_NUM 2

static void thread_exit(thread_pool_t *pool);

// create pool
thread_pool_t *thread_pool_create(int thread_min, int thread_max, int queue_capacity)
{
    thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));

    do
    {
        if (pool == NULL)
        {
            printf("thread pool malloc failed\n");
            break;
        }

        pool->thread_id = (pthread_t *)malloc(sizeof(pthread_t) * thread_max);
        if (pool->thread_id == NULL)
        {
            printf("thread id malloc failed\n");
            break;
        }
        memset(pool->thread_id, 0, sizeof(pthread_t) * thread_max);

        pool->thread_max = thread_max;
        pool->thread_min = thread_min;
        pool->thread_busy = 0;
        pool->thread_alive = thread_min;
        pool->thread_exit = 0;

        if (pthread_mutex_init(&pool->mutex_pool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutex_busy, NULL) != 0 ||
            pthread_cond_init(&pool->notempty, NULL) != 0 ||
            pthread_cond_init(&pool->notfull, NULL) != 0)
        {
            printf("pthread mutex or cond init failed\n");
            break;
        }

        pool->task_queue = (task_t *)malloc(sizeof(task_t) * queue_capacity);
        if (pool->task_queue == NULL)
        {
            printf("task queue malloc failed\n");
            break;
        }

        pool->queue_capacity = queue_capacity;
        pool->queue_size = 0;
        pool->queue_front = 0;
        pool->queue_rear = 0;

        pool->shutdown = 0;

        pthread_create(&pool->manage_id, NULL, manager_routine, pool);

        for (int i = 0; i < thread_min; i++)
        {
            pthread_create(&pool->thread_id[i], NULL, worker_routine, pool);
        }

        printf("thread pool create success\n");

        return pool;

    } while (0);

    // free resource
    if (pool->thread_id != NULL)
    {
        free(pool->thread_id);
        pool->thread_id = NULL;
    }

    if (pool->task_queue != NULL)
    {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }

    if (pool != NULL)
    {
        free(pool);
        pool = NULL;
    }

    return NULL;
}

int thread_pool_destroy(thread_pool_t *pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    pool->shutdown = 1;

    // wait manager routine exit
    pthread_join(pool->manage_id, NULL);

    for (int i = 0; i < pool->thread_alive; i++)
    {
        // wake work thread
        pthread_cond_signal(&pool->notempty);
    }

    sleep(1);

    // free resource
    if (pool->thread_id != NULL)
    {
        free(pool->thread_id);
        pool->thread_id = NULL;
    }

    if (pool->task_queue != NULL)
    {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }

    pthread_mutex_destroy(&pool->mutex_busy);
    pthread_mutex_destroy(&pool->mutex_pool);
    pthread_cond_destroy(&pool->notempty);
    pthread_cond_destroy(&pool->notfull);

    if (pool != NULL)
    {
        free(pool);
        pool = NULL;
    }

    printf("thread pool destroy success\n");

    return 0;
}

void *worker_routine(void *arg)
{
    printf("thread %ld created\n", pthread_self());

    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1)
    {

        pthread_mutex_lock(&pool->mutex_pool);

        while (pool->queue_size == 0 && pool->shutdown == 0)
        {
            // block worker routine
            pthread_cond_wait(&pool->notempty, &pool->mutex_pool);

            if (pool->thread_exit > 0)
            {
                pool->thread_exit--;
                if (pool->thread_alive > pool->thread_min)
                {
                    pool->thread_alive--;
                    pthread_mutex_unlock(&pool->mutex_pool);
                    thread_exit(pool);
                }
            }
        }

        if (pool->shutdown != 0)
        {
            pthread_mutex_unlock(&pool->mutex_pool);
            thread_exit(pool);
        }

        // consume worker thread
        task_t task;
        task.fun = pool->task_queue[pool->queue_front].fun;
        task.arg = pool->task_queue[pool->queue_front].arg;

        pool->queue_front = (pool->queue_front + 1) % pool->queue_capacity;
        pool->queue_size--;

        // wake producer
        pthread_cond_signal(&pool->notfull);

        pthread_mutex_unlock(&pool->mutex_pool);

        pthread_mutex_lock(&pool->mutex_busy);
        pool->thread_busy++;
        pthread_mutex_unlock(&pool->mutex_busy);

        printf("thread %ld start working\n", pthread_self());

        (*task.fun)(task.arg);
        free(task.arg);
        task.arg = NULL;

        pthread_mutex_lock(&pool->mutex_busy);
        pool->thread_busy--;
        pthread_mutex_unlock(&pool->mutex_busy);
    }

    return NULL;
}

void *manager_routine(void *arg)
{

    thread_pool_t *pool = (thread_pool_t *)arg;

    while (pool->shutdown == 0)
    {
        sleep(3);

        pthread_mutex_lock(&pool->mutex_pool);
        int queue_size = pool->queue_size;
        int thread_alive = pool->thread_alive;
        int thread_max = pool->thread_max;
        int thread_min = pool->thread_min;
        pthread_mutex_unlock(&pool->mutex_pool);

        pthread_mutex_lock(&pool->mutex_busy);
        int thread_busy = pool->thread_busy;
        pthread_mutex_unlock(&pool->mutex_busy);

        // add thread
        // condition 1 : alive work thread num < task num
        // condition 2 : alive work thread num < max work thread num
        if ((queue_size > thread_alive) && (thread_alive < thread_max))
        {
            pthread_mutex_lock(&pool->mutex_pool);

            int add_counter = 0;

            for (int i = 0; i < pool->thread_max; i++)
            {

                if (!(add_counter < THREAD_ADD_NUM))
                {
                    break;
                }

                if (!(pool->thread_alive < pool->thread_max))
                {
                    break;
                }

                if (pool->thread_id[i] != 0)
                {
                    continue;
                }

                pthread_create(&pool->thread_id[i], NULL, worker_routine, pool);
                add_counter++;
                pool->thread_alive++;
            }

            pthread_mutex_unlock(&pool->mutex_pool);

            // add thread continue ignore destroy thread
            continue;
        }

        // destroy thread
        // condition 1 : busy work thread num * 2 < alive work thread num
        // condition 2 : min work thread num < alive work thread num
        if ((thread_busy * 2 < thread_alive) && (thread_alive > thread_min))
        {
            pthread_mutex_lock(&pool->mutex_pool);

            pool->thread_exit = THREAD_DESTROY_NUM;

            pthread_mutex_unlock(&pool->mutex_pool);

            for (int i = 0; i < THREAD_DESTROY_NUM; i++)
            {
                pthread_cond_signal(&pool->notempty);
            }
        }
    }
    
    printf("manager thread exit\n");

    return NULL;
}

// clear thread_id
static void thread_exit(thread_pool_t *pool)
{
    pthread_t tid = pthread_self();

    for (int i = 0; i < pool->thread_max; i++)
    {
        if (pool->thread_id[i] == tid)
        {
            pool->thread_id[i] = 0;
            printf("thread_exit called %ld exiting...\n", tid);
            break;
        }
    }

    pthread_exit(NULL);
}

// add task
void thread_pool_add_task(thread_pool_t *pool, void (*fun)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mutex_pool);
    while (pool->queue_size == pool->queue_capacity && pool->shutdown == 0)
    {
        // block producer routine
        pthread_cond_wait(&pool->notfull, &pool->mutex_pool);
    }

    if (pool->shutdown == 1)
    {
        pthread_mutex_unlock(&pool->mutex_pool);
        return;
    }

    // add task

    pool->task_queue[pool->queue_rear].fun = fun;
    pool->task_queue[pool->queue_rear].arg = arg;

    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_capacity;
    pool->queue_size++;

    // wake consumer
    pthread_cond_signal(&pool->notempty);

    pthread_mutex_unlock(&pool->mutex_pool);
}

// get thread busy
int thread_pool_get_busy(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mutex_busy);
    int thread_busy = pool->thread_busy;
    pthread_mutex_unlock(&pool->mutex_busy);

    return thread_busy;
}

// get thread alive
int thread_pool_get_alive(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mutex_pool);
    int thread_alive = pool->thread_alive;
    pthread_mutex_unlock(&pool->mutex_busy);
    return thread_alive;
}