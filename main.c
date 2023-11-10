#include <stdio.h>
#include "thread_pool.h"

int main()
{
    thread_pool_t *pool = thread_pool_create(3, 10, 100);

    printf("hello world\n");

    return 0;
}