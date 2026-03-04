#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>

typedef struct task_t{
    void* args;
    void* (*task_func)(void*);
} task_t;

typedef struct node_t{
    task_t* task;
    struct node_t* next;
} node_t;

typedef struct thread_pool_t thread_pool_t;


thread_pool_t* init_thread_pool();
void destroy_thread_pool(thread_pool_t* pool);

void terminate_thread_pool(thread_pool_t* pool);
int isTerminated(thread_pool_t* pool);

void enqueue_task(thread_pool_t* pool, task_t* task);
task_t* dequeue_task(thread_pool_t* pool);

#endif /* THREAD_POOL_H */