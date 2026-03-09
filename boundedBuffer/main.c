#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "boundedBuffer.h"
#include "producer_consumer.h"
#include "task.h"

#define MAX_THREADS 64
#define TASK_COUNT 1000
#define BUFFER_CAPACITY 100

typedef struct {
    double time_takenMonotonic;
    double time_takenProcess;
    double time_takenPerTask;
    double cpuTimeVsWallTime;
} run_metrics_t;

static void run_one(int type, int num_consumers, int buffer_cap, run_metrics_t* out) {
    pthread_t consumer_threads[MAX_THREADS];
    pthread_t producer_thread;

    bounded_buffer_t* buf = init_bounded_buffer(buffer_cap);

    void* producer_args[3];
    producer_args[0] = buf;
    producer_args[1] = (void*)(intptr_t)type;
    producer_args[2] = (void*)(intptr_t)TASK_COUNT;

    struct timespec startMono, endMono, startProc, endProc;
    clock_gettime(CLOCK_MONOTONIC, &startMono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProc);

    void* consumer_args[MAX_THREADS][2];
    for (int i = 0; i < num_consumers; i++) {
        consumer_args[i][0] = buf;
        consumer_args[i][1] = (void*)(intptr_t)i;
        pthread_create(&consumer_threads[i], NULL, consumer, consumer_args[i]);
    }

    pthread_create(&producer_thread, NULL, producer, producer_args);

    pthread_join(producer_thread, NULL);
    for (int i = 0; i < num_consumers; i++)
        pthread_join(consumer_threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &endMono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProc);

    out->time_takenMonotonic = (endMono.tv_sec - startMono.tv_sec) +
        (endMono.tv_nsec - startMono.tv_nsec) / 1e9;
    out->time_takenProcess = (endProc.tv_sec - startProc.tv_sec) +
        (endProc.tv_nsec - startProc.tv_nsec) / 1e9;
    out->time_takenPerTask = (TASK_COUNT > 0) ? (out->time_takenMonotonic / TASK_COUNT) : 0.0;
    out->cpuTimeVsWallTime = (out->time_takenMonotonic > 0) ?
        (out->time_takenProcess / out->time_takenMonotonic) : 0.0;

    destroy_bounded_buffer(buf);
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
    int buffer_cap = BUFFER_CAPACITY;

    if (argc >= 2) {
        num_consumers = atoi(argv[1]);
        if (num_consumers < 1 || num_consumers > MAX_THREADS) {
            fprintf(stderr, "Usage: %s [num_consumers] [buffer_capacity]\n", argv[0]);
            return 1;
        }
    }
    if (argc >= 3)
        buffer_cap = atoi(argv[2]);

    printf("Bounded Buffer: %d consumers, buffer capacity %d, %d tasks per workload\n",
           num_consumers, buffer_cap, TASK_COUNT);

    run_metrics_t metrics;
    const char* labels[] = { "Small", "Heavy", "File I/O", "Blocking", "Mixed" };
    int types[] = { 1, 2, 3, 4, 0 };

    for (int i = 0; i < 5; i++) {
        run_one(types[i], num_consumers, buffer_cap, &metrics);
        log_metrics(labels[i], &metrics, TASK_COUNT);
    }

    printf("\nDone.\n");
    return 0;
}
