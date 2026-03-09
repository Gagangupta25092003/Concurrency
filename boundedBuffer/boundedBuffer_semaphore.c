#include "boundedBuffer.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

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
    sem_t* empty_slots;
    sem_t* filled_slots;
};

static int sem_id_counter = 0;

static sem_t* open_sem(int value) {
    char name[48];
    int id = __sync_fetch_and_add(&sem_id_counter, 1);
    snprintf(name, sizeof(name), "/bb_%d_%d", getpid(), id);
    sem_unlink(name);
    sem_t* s = sem_open(name, O_CREAT, 0644, value);
    sem_unlink(name);
    return s;
}

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
    buf->empty_slots = open_sem(capacity);
    buf->filled_slots = open_sem(0);
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
    sem_close(buf->empty_slots);
    sem_close(buf->filled_slots);
    free(buf);
}

void bb_push(bounded_buffer_t* buf, task_t* item) {
    sem_wait(buf->empty_slots);

    if (buf->is_terminated) {
        sem_post(buf->empty_slots);
        return;
    }

    pthread_mutex_lock(&buf->lock);
    node_t* node = create_node(item);
    if (buf->tail_node) {
        buf->tail_node->next = node;
    } else {
        buf->head_node = node;
    }
    buf->tail_node = node;
    buf->filled++;
    pthread_mutex_unlock(&buf->lock);

    sem_post(buf->filled_slots);
}

task_t* bb_pop(bounded_buffer_t* buf) {
    sem_wait(buf->filled_slots);

    pthread_mutex_lock(&buf->lock);
    if (buf->filled == 0) {
        pthread_mutex_unlock(&buf->lock);
        sem_post(buf->filled_slots);
        return NULL;
    }

    node_t* node = buf->head_node;
    task_t* task = node->task;
    buf->head_node = node->next;
    if (!buf->head_node)
        buf->tail_node = NULL;
    buf->filled--;
    pthread_mutex_unlock(&buf->lock);

    sem_post(buf->empty_slots);
    free(node);
    return task;
}

void terminate(bounded_buffer_t* buf) {
    buf->is_terminated = 1;
    sem_post(buf->filled_slots);
}

int is_terminated(bounded_buffer_t* buf) {
    return buf->is_terminated;
}
