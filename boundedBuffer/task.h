#ifndef TASK
#define TASK

#include <stdlib.h>

typedef struct task_t task_t;

task_t* create_task(void* (*task_func)(void*), void* args);
void destroy_task(task_t* task);
void execute_task(task_t* task);

#endif
