#include "producer_consumer.h"
#include "boundedBuffer.h"
#include "tasks.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void enqueue_n(bounded_buffer_t* buf, void* (*task_func)(void*), int n) {
    for (int i = 0; i < n; i++) {
        task_t* task = create_task(task_func, NULL);
        bb_push(buf, task);
    }
}

void* producer(void* arg) {
    void** args = (void**)arg;
    bounded_buffer_t* buf = (bounded_buffer_t*)args[0];
    int type = (int)(intptr_t)args[1];
    int count = (int)(intptr_t)args[2];

    switch (type) {
        case 1: enqueue_n(buf, task_1, count); break;
        case 2: enqueue_n(buf, task_2, count); break;
        case 3: enqueue_n(buf, task_4, count); break;
        case 4: enqueue_n(buf, task_3, count); break;
        default: {
            int each = count / 4;
            enqueue_n(buf, task_1, each);
            enqueue_n(buf, task_2, each);
            enqueue_n(buf, task_3, each);
            enqueue_n(buf, task_4, each);
            break;
        }
    }

    terminate(buf);
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
