#ifndef BOUNDED_BUFFER
#define BOUNDED_BUFFER

#include "task.h"

typedef struct bounded_buffer_t bounded_buffer_t;

bounded_buffer_t* init_bounded_buffer(int capacity);
void destroy_bounded_buffer(bounded_buffer_t* buf);

void bb_push(bounded_buffer_t* buf, task_t* item);
task_t* bb_pop(bounded_buffer_t* buf);

void terminate(bounded_buffer_t* buf);
int is_terminated(bounded_buffer_t* buf);

#endif
