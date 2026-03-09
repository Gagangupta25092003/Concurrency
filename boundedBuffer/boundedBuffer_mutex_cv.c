#include "boundedBuffer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct node_t {
    task_t* task;
    struct node_t* next;
} node_t;

struct bounded_buffer_t {
    int capacity;
    int filled;
    int is_terminated;

    node_t* head_node;
    node_t* tail_node;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

static node_t* create_node(task_t* task) {
    node_t* node = malloc(sizeof(node_t));
    node->next = NULL;
    node->task = task;
    return node;
}

bounded_buffer_t* init_bounded_buffer(int capacity) {
    bounded_buffer_t* buf = malloc(sizeof(bounded_buffer_t));
    buf->capacity = capacity;
    buf->filled = 0;
    buf->is_terminated = 0;
    buf->head_node = NULL;
    buf->tail_node = NULL;
    pthread_mutex_init(&buf->lock, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    return buf;
}

void destroy_bounded_buffer(bounded_buffer_t* buf) {
    pthread_mutex_lock(&buf->lock);
    while (buf->head_node != NULL) {
        node_t* temp = buf->head_node;
        buf->head_node = temp->next;
        destroy_task(temp->task);
        free(temp);
    }
    pthread_mutex_unlock(&buf->lock);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->not_empty);
    pthread_cond_destroy(&buf->not_full);
    free(buf);
}

void bb_push(bounded_buffer_t* buf, task_t* item) {
    pthread_mutex_lock(&buf->lock);
    while (buf->filled >= buf->capacity && !buf->is_terminated)
        pthread_cond_wait(&buf->not_full, &buf->lock);

    if (buf->is_terminated) {
        pthread_mutex_unlock(&buf->lock);
        return;
    }

    node_t* node = create_node(item);
    if (buf->tail_node) {
        buf->tail_node->next = node;
    } else {
        buf->head_node = node;
    }
    buf->tail_node = node;
    buf->filled++;

    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->lock);
}

task_t* bb_pop(bounded_buffer_t* buf) {
    pthread_mutex_lock(&buf->lock);
    while (buf->filled == 0 && !buf->is_terminated)
        pthread_cond_wait(&buf->not_empty, &buf->lock);

    if (buf->filled == 0) {
        pthread_cond_broadcast(&buf->not_empty);
        pthread_mutex_unlock(&buf->lock);
        return NULL;
    }

    node_t* node = buf->head_node;
    task_t* task = node->task;
    buf->head_node = node->next;
    if (!buf->head_node)
        buf->tail_node = NULL;
    buf->filled--;

    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->lock);
    free(node);
    return task;
}

void terminate(bounded_buffer_t* buf) {
    pthread_mutex_lock(&buf->lock);
    buf->is_terminated = 1;
    pthread_cond_broadcast(&buf->not_empty);
    pthread_cond_broadcast(&buf->not_full);
    pthread_mutex_unlock(&buf->lock);
}

int is_terminated(bounded_buffer_t* buf) {
    pthread_mutex_lock(&buf->lock);
    int val = buf->is_terminated;
    pthread_mutex_unlock(&buf->lock);
    return val;
}
