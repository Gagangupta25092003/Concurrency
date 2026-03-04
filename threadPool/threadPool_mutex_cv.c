#include "threadPool.h"

struct thread_pool_t{
    node_t* head_node;
    node_t* tail_node;
    int is_running;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

thread_pool_t * init_thread_pool(){
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    pool->head_node = NULL;
    pool->tail_node = NULL;
    pool->is_running = 1;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);
    return pool;
}

void destroy_thread_pool(thread_pool_t* pool){
    pthread_mutex_lock(&pool->mutex);
    node_t* curNode = pool->head_node;
    while(curNode != NULL){
        node_t* temp = curNode;
        curNode = curNode->next;
        free(temp);
    }
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

void terminate_thread_pool(thread_pool_t* pool){
    pthread_mutex_lock(&pool->mutex);
    pool->is_running = 0;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

int isTerminated(thread_pool_t* pool){
    pthread_mutex_lock(&pool->mutex);
    int terminated = !pool->is_running;
    pthread_mutex_unlock(&pool->mutex);
    return terminated;
}

void enqueue_task(thread_pool_t* pool, task_t* task){
    node_t *newNode = (node_t *)malloc(sizeof(node_t));
    assert(newNode!=NULL);
    newNode->task = task;
    newNode->next = NULL;
    pthread_mutex_lock(&pool->mutex);
    if(pool->tail_node == NULL){
        pool->head_node = newNode;
        pool->tail_node = newNode;
    }
    else{
        pool->tail_node->next = newNode;
        pool->tail_node = newNode;
    }
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

task_t* dequeue_task(thread_pool_t* pool){
    pthread_mutex_lock(&pool->mutex);
    while(pool->head_node == NULL && pool->is_running){
        pthread_cond_wait(&pool->cond, &pool->mutex);
    }
    if(!pool->is_running && pool->head_node == NULL){
        pthread_mutex_unlock(&pool->mutex);
        return NULL; // termination signal
    }
    else{
        node_t * oldHead = pool->head_node;
        task_t * task = oldHead->task;
        pool->head_node = pool->head_node->next;
        if(pool->head_node == NULL){
            pool->tail_node = NULL;
        }
        pthread_mutex_unlock(&pool->mutex);

        free(oldHead);
        return task;
    }
}