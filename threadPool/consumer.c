#include "consumer.h"
#include <stdint.h>

void* consumer(void* arg){
    void ** args = (void**)arg;
    thread_pool_t *pool = (thread_pool_t *)args[0];
    int id = (int)(intptr_t)args[1];
    assert(pool != NULL);
    printf("(Consumer) ID: %d started\n", id);
    while(1){
        task_t *task = dequeue_task(pool);
        if(task != NULL){
            task->task_func(task->args);
            free(task);
        } else if (isTerminated(pool)) {
            break;
        }
    }
    return NULL;
}