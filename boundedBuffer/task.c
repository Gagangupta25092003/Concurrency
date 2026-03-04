#include "task.h"

struct task_t{
    void* (* taskfunc) (void*);
    void* args;
};

task_t* create_task(void* (*task_func) (void*), void* args){
    task_t* task_t = malloc(sizeof(task_t));
    return task_t;
}
void destroy_task(task_t* task){
    free(task);
}