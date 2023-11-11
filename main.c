#include <stdio.h>
#include "thread_pool.h"

#define TEST_TASK_NUM 10

void test_fun(void *arg)
{
    int num = *(int *)arg;
    printf("thread %ld is working ,number = %d\n", pthread_self(), num);
    sleep(1);
}

/*      step1: create a company      */
/*      step2: hire three empolyees     */
/*      step3: add ten tasks      */
/*      step4: hire two more empolyees raise efficiency      */
/*      step5: dismiss all employees after complete the work     */
/*      step6: destroy the company     */

int main()
{
    printf("hello thread pool\n");

    thread_pool_t *pool = thread_pool_create(3, 10, 10);

    sleep(1);

    for (int i = 0; i < TEST_TASK_NUM; i++)
    {
        int *num = (int *)malloc(sizeof(int));
        *num = i + 2000;
        thread_pool_add_task(pool, test_fun, num);
    }

    // other work
    sleep(5);

    thread_pool_destroy(pool);

    return 0;
}