#include "boundedBuffer.h"
#include "task.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>

static BUFFER_CAPACITY = 100;

typedef struct node_t{
    task_t *task;
    node_t *next;
} node_t;

struct bounded_buffer_t{
    int filled;
    int terminate;

    node_t* head_node;
    node_t* tail_node;

    pthread_mutex_t lock;
    pthread_cond_t empty;
    pthread_cond_t full;
};

static node_t* create_node(task_t* task){
    node_t *node = malloc(sizeof(node_t *));
    
    node->next = NULL;
    node->task = task;

    return node;
}

static void destroy_node(node_t * node){
    destroy_task(node->task);
    free(node);
}

bounded_buffer_t* init_bounded_buffer(){
    bounded_buffer_t* buf = (bounded_buffer_t *)malloc(sizeof(bounded_buffer_t));

    buf->head_node = NULL;
    buf->tail_node = NULL;

    buf->filled = 0;
    buf->terminate = 0;

    pthread_mutex_init(&buf->lock, NULL);

    pthread_cond_init(&buf->empty, NULL);
    pthread_cond_init(&buf->full, NULL);

    return buf;
};

void destroy_bounded_buffer(bounded_buffer_t* buf){
    pthread_mutex_lock(&buf->lock);
    while(buf->head_node != NULL){
        node_t *temp = buf->head_node;
        buf->head_node = temp->next;
        destroy_node(temp);
    }
    pthread_mutex_unlock(&buf->lock);

    pthread_mutex_destroy(&buf->lock);

    pthread_cond_destroy(&buf->empty);
    pthread_cond_destroy(&buf->full);

    free(buf);
    return NULL;
};

void bb_push(bounded_buffer_t* buf, task_t* item){
    if(buf->filled < BUFFER_CAPACITY){
        node_t* node = create_node(item);
        buf->tail_node->next = node;
        buf->tail_node = node;
        buf->filled++;
    }
}   

task_t* bb_pop(bounded_buffer_t* buf){
    task_t* task;
    if(buf->filled != 0){
        node_t *node = buf->head_node;
        task = node->task;
        buf->head_node = buf->head_node->next;
        buf->filled--;
        destroy_node(node);
    }
    return task;
}

void terminate(bounded_buffer_t *buf){
    buf->terminate = 1;
}