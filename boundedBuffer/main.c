#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "boundedBuffer.h"
#include "producer_consumer.h"
#include "tasks.h"
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

static void compute_metrics(struct timespec* s_mono, struct timespec* e_mono,
                            struct timespec* s_proc, struct timespec* e_proc,
                            int total_tasks, run_metrics_t* out) {
    out->time_takenMonotonic = (e_mono->tv_sec - s_mono->tv_sec) +
        (e_mono->tv_nsec - s_mono->tv_nsec) / 1e9;
    out->time_takenProcess = (e_proc->tv_sec - s_proc->tv_sec) +
        (e_proc->tv_nsec - s_proc->tv_nsec) / 1e9;
    out->time_takenPerTask = (total_tasks > 0)
        ? (out->time_takenMonotonic / total_tasks) : 0.0;
    out->cpuTimeVsWallTime = (out->time_takenMonotonic > 0)
        ? (out->time_takenProcess / out->time_takenMonotonic) : 0.0;
}

static void run_single_type(void* (*task_func)(void*), int total_tasks,
                            int num_producers, int num_consumers,
                            int buffer_cap, run_metrics_t* out) {
    bounded_buffer_t* buf = init_bounded_buffer(buffer_cap);

    struct timespec s_mono, e_mono, s_proc, e_proc;
    clock_gettime(CLOCK_MONOTONIC, &s_mono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s_proc);

    pthread_t cons[MAX_THREADS];
    void* c_args[MAX_THREADS][2];
    for (int i = 0; i < num_consumers; i++) {
        c_args[i][0] = buf;
        c_args[i][1] = (void*)(intptr_t)i;
        pthread_create(&cons[i], NULL, consumer, c_args[i]);
    }

    pthread_t prods[MAX_THREADS];
    void* p_args[MAX_THREADS][3];
    int base = total_tasks / num_producers;
    int extra = total_tasks % num_producers;
    for (int i = 0; i < num_producers; i++) {
        int count = base + (i < extra ? 1 : 0);
        p_args[i][0] = buf;
        p_args[i][1] = (void*)task_func;
        p_args[i][2] = (void*)(intptr_t)count;
        pthread_create(&prods[i], NULL, producer, p_args[i]);
    }

    for (int i = 0; i < num_producers; i++)
        pthread_join(prods[i], NULL);
    terminate(buf);

    for (int i = 0; i < num_consumers; i++)
        pthread_join(cons[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &e_mono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &e_proc);

    compute_metrics(&s_mono, &e_mono, &s_proc, &e_proc, total_tasks, out);
    destroy_bounded_buffer(buf);
}

static void run_mixed(int total_tasks, int num_producers, int num_consumers,
                      int buffer_cap, run_metrics_t* out) {
    bounded_buffer_t* buf = init_bounded_buffer(buffer_cap);

    struct timespec s_mono, e_mono, s_proc, e_proc;
    clock_gettime(CLOCK_MONOTONIC, &s_mono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &s_proc);

    pthread_t cons[MAX_THREADS];
    void* c_args[MAX_THREADS][2];
    for (int i = 0; i < num_consumers; i++) {
        c_args[i][0] = buf;
        c_args[i][1] = (void*)(intptr_t)i;
        pthread_create(&cons[i], NULL, consumer, c_args[i]);
    }

    pthread_t prods[MAX_THREADS];
    void* p_args[MAX_THREADS][2];
    int base = total_tasks / num_producers;
    int extra = total_tasks % num_producers;
    for (int i = 0; i < num_producers; i++) {
        int count = base + (i < extra ? 1 : 0);
        p_args[i][0] = buf;
        p_args[i][1] = (void*)(intptr_t)count;
        pthread_create(&prods[i], NULL, mixed_producer, p_args[i]);
    }

    for (int i = 0; i < num_producers; i++)
        pthread_join(prods[i], NULL);
    terminate(buf);

    for (int i = 0; i < num_consumers; i++)
        pthread_join(cons[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &e_mono);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &e_proc);

    compute_metrics(&s_mono, &e_mono, &s_proc, &e_proc, total_tasks, out);
    destroy_bounded_buffer(buf);
}

static void log_metrics(const char* label, const char* category,
                        const char* task_desc, const run_metrics_t* m,
                        int task_count, int num_prod, int num_cons) {
    printf("\n--- %s [%s] (%d tasks, %d producers, %d consumers) ---\n",
           label, category, task_count, num_prod, num_cons);
    printf("  Task: %s\n", task_desc);
    printf("  time_takenMonotonic (wall, s):  %.6f\n", m->time_takenMonotonic);
    printf("  time_takenProcess (CPU, s):     %.6f\n", m->time_takenProcess);
    printf("  time_takenPerTask (wall, s):    %.9f\n", m->time_takenPerTask);
    printf("  cpuTimeVsWallTime (ratio):      %.4f\n", m->cpuTimeVsWallTime);
}

int main(int argc, char** argv) {
    int num_producers = 2;
    int num_consumers = 4;
    int buffer_cap = BUFFER_CAPACITY;

    if (argc >= 2) num_producers = atoi(argv[1]);
    if (argc >= 3) num_consumers = atoi(argv[2]);
    if (argc >= 4) buffer_cap = atoi(argv[3]);

    if (num_producers < 1 || num_producers > MAX_THREADS ||
        num_consumers < 1 || num_consumers > MAX_THREADS) {
        fprintf(stderr,
            "Usage: %s [num_producers] [num_consumers] [buffer_capacity]\n",
            argv[0]);
        return 1;
    }

    printf("Bounded Buffer: %d producers, %d consumers, "
           "buffer capacity %d, %d tasks per workload\n",
           num_producers, num_consumers, buffer_cap, TASK_COUNT);

    run_metrics_t m;

    printf("\n============================================================");
    printf("\n  LIGHT TASKS (low CPU, quick or blocking)");
    printf("\n============================================================");

    run_single_type(task_1, TASK_COUNT, num_producers, num_consumers, buffer_cap, &m);
    log_metrics("Small CPU", "light",
                "~100 arithmetic iterations per task",
                &m, TASK_COUNT, num_producers, num_consumers);

    run_single_type(task_3, TASK_COUNT, num_producers, num_consumers, buffer_cap, &m);
    log_metrics("Sleep / Blocking", "light",
                "10ms usleep per task",
                &m, TASK_COUNT, num_producers, num_consumers);

    printf("\n\n============================================================");
    printf("\n  HEAVY TASKS (CPU-intensive or I/O-bound)");
    printf("\n============================================================");

    run_single_type(task_2, TASK_COUNT, num_producers, num_consumers, buffer_cap, &m);
    log_metrics("Heavy CPU", "heavy",
                "~80ms CPU-bound busy loop per task",
                &m, TASK_COUNT, num_producers, num_consumers);

    run_single_type(task_4, TASK_COUNT, num_producers, num_consumers, buffer_cap, &m);
    log_metrics("File I/O", "heavy",
                "temp file create, write, read, unlink per task",
                &m, TASK_COUNT, num_producers, num_consumers);

    printf("\n\n============================================================");
    printf("\n  MIXED WORKLOAD (250 of each task type)");
    printf("\n============================================================");

    run_mixed(TASK_COUNT, num_producers, num_consumers, buffer_cap, &m);
    log_metrics("Mixed", "light + heavy",
                "250 Small + 250 Heavy + 250 Sleep + 250 File I/O",
                &m, TASK_COUNT, num_producers, num_consumers);

    printf("\nDone.\n");
    return 0;
}
