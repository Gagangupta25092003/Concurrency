#include "producer_consumer.h"
#include "boundedBuffer.h"
#include "tasks.h"
#include "task.h"
#include <stdlib.h>
#include <stdint.h>

void* producer(void* arg) {
    void** args = (void**)arg;
    bounded_buffer_t* buf = (bounded_buffer_t*)args[0];
    void* (*task_func)(void*) = args[1];
    int count = (int)(intptr_t)args[2];

    for (int i = 0; i < count; i++)
        bb_push(buf, create_task(task_func, NULL));

    return NULL;
}

void* mixed_producer(void* arg) {
    void** args = (void**)arg;
    bounded_buffer_t* buf = (bounded_buffer_t*)args[0];
    int count = (int)(intptr_t)args[1];

    void* (*funcs[4])(void*) = { task_1, task_2, task_3, task_4 };
    int per_type = count / 4;

    for (int f = 0; f < 4; f++)
        for (int i = 0; i < per_type; i++)
            bb_push(buf, create_task(funcs[f], NULL));

    return NULL;
}

void* consumer(void* arg) {
    void** args = (void**)arg;
    bounded_buffer_t* buf = (bounded_buffer_t*)args[0];

    while (1) {
        task_t* task = bb_pop(buf);
        if (task == NULL)
            break;
        execute_task(task);
        destroy_task(task);
    }
    return NULL;
}
