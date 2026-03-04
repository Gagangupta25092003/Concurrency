#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>

#include "tasks.h"
#include "threadPool.h"
#include "consumer.h"

#define MAX_THREADS 64
#define TASK_COUNT 1000
#define PRODUCER_DELAY_MS 0

static int totalTask;

static task_t* make_task(void* (*task_func)(void*), void* args) {
    task_t* task = malloc(sizeof(task_t));
    assert(task != NULL);
    task->task_func = task_func;
    task->args = args;
    totalTask++;
    return task;
}

static void enqueue_n(thread_pool_t* pool, void* (*task_func)(void*), int n) {
    for (int i = 0; i < n; i++) {
        task_t* task = make_task(task_func, NULL);
        enqueue_task(pool, task);
        usleep(PRODUCER_DELAY_MS * 1000);  /* 100ms after each enqueue */
    }
}

static void* producer(void* arg) {
    void** args = (void**)arg;
    thread_pool_t* pool = (thread_pool_t*)args[0];
    int type = (int)(intptr_t)args[1];
    assert(pool != NULL);

    switch (type) {
        case 1:
            enqueue_n(pool, task_1, TASK_COUNT);
            break;
        case 2:
            enqueue_n(pool, task_2, TASK_COUNT);
            break;
        case 3:
            enqueue_n(pool, task_4, TASK_COUNT);  /* file I/O */
            break;
        case 4:
            enqueue_n(pool, task_3, TASK_COUNT);  /* blocking / sleeping */
            break;
        default: {
            int each = TASK_COUNT / 4;
            enqueue_n(pool, task_1, each);
            enqueue_n(pool, task_2, each);
            enqueue_n(pool, task_3, each);
            enqueue_n(pool, task_4, each);
            break;
        }
    }

    terminate_thread_pool(pool);
    return NULL;
}

typedef struct {
    double time_takenMonotonic;
    double time_takenProcess;
    double time_takenPerTask;
    double cpuTimeVsWallTime;
} run_metrics_t;

static void run_one_task_type(int type, const char* label, int num_consumers, run_metrics_t* out) {
    pthread_t consumerThreads[MAX_THREADS];
    pthread_t producerThread;

    if (num_consumers < 1 || num_consumers > MAX_THREADS) {
        fprintf(stderr, "Invalid num_consumers: %d (must be 1..%d)\n", num_consumers, MAX_THREADS);
        out->time_takenMonotonic = out->time_takenProcess = out->time_takenPerTask = out->cpuTimeVsWallTime = 0;
        return;
    }

    thread_pool_t* pool = init_thread_pool();
    totalTask = 0;

    void* producer_args[2];
    producer_args[0] = pool;
    producer_args[1] = (void*)(intptr_t)type;

    struct timespec startMonotonic, endMonotonic;
    struct timespec startProcess, endProcess;

    clock_gettime(CLOCK_MONOTONIC, &startMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProcess);

    pthread_create(&producerThread, NULL, producer, producer_args);

    void* consumer_args[MAX_THREADS][2];
    for (int i = 0; i < num_consumers; i++) {
        consumer_args[i][0] = pool;
        consumer_args[i][1] = (void*)(intptr_t)i;
        pthread_create(&consumerThreads[i], NULL, consumer, consumer_args[i]);
    }

    pthread_join(producerThread, NULL);
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumerThreads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &endMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProcess);

    out->time_takenMonotonic = (endMonotonic.tv_sec - startMonotonic.tv_sec) +
        (endMonotonic.tv_nsec - startMonotonic.tv_nsec) / 1e9;
    out->time_takenProcess = (endProcess.tv_sec - startProcess.tv_sec) +
        (endProcess.tv_nsec - startProcess.tv_nsec) / 1e9;
    out->time_takenPerTask = (totalTask > 0) ? (out->time_takenMonotonic / totalTask) : 0.0;
    out->cpuTimeVsWallTime = (out->time_takenMonotonic > 0) ? (out->time_takenProcess / out->time_takenMonotonic) : 0.0;

    destroy_thread_pool(pool);
}

static void log_metrics(const char* label, const run_metrics_t* m, int task_count) {
    printf("\n--- %s (%d tasks) ---\n", label, task_count);
    printf("  time_takenMonotonic (wall, s):  %.6f\n", m->time_takenMonotonic);
    printf("  time_takenProcess (CPU, s):     %.6f\n", m->time_takenProcess);
    printf("  time_takenPerTask (wall, s):    %.9f\n", m->time_takenPerTask);
    printf("  cpuTimeVsWallTime (ratio):      %.4f\n", m->cpuTimeVsWallTime);
}

int main(int argc, char** argv) {
    int num_consumers = 4;
    if (argc >= 2) {
        num_consumers = atoi(argv[1]);
        if (num_consumers < 1 || num_consumers > MAX_THREADS) {
            fprintf(stderr, "Usage: %s [num_consumer_threads]\n", argv[0]);
            fprintf(stderr, "  num_consumer_threads must be 1..%d (default: 4)\n", MAX_THREADS);
            return 1;
        }
    }

    run_metrics_t metrics;
    
    /* Original: all 5 task types */
    const char* labels[] = { "Small", "Heavy", "File I/O", "Blocking", "Mixed" };
    int types[] = { 1, 2, 3, 4, 0 };
    for (int i = 0; i < 5; i++) {
        run_one_task_type(types[i], labels[i], num_consumers, &metrics);
        log_metrics(labels[i], &metrics, TASK_COUNT);
    }

    printf("\nDone.\n");
    return 0;
}
//Add Readme.md in threadPool in that add Description of this threadPool and how to run it. Also add all the graphs desribing all the key inferences from it.  