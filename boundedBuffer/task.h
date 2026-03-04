#ifndef TASK
#define TASK
typedef struct task_t task_t;
task_t* create_task(void* (*task_func) (void*), void* args);
void destroy_task(task_t* task);
#endif