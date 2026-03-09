#include "task.h"
#include <stdlib.h>

struct task_t {
    void* (*task_func)(void*);
    void* args;
};

task_t* create_task(void* (*task_func)(void*), void* args) {
    task_t* t = malloc(sizeof(task_t));
    t->task_func = task_func;
    t->args = args;
    return t;
}

void destroy_task(task_t* task) {
    free(task);
}

void execute_task(task_t* task) {
    if (task && task->task_func)
        task->task_func(task->args);
}
