#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

volatile int counter = 0;
volatile int lock = 0;
pthread_mutex_t mutex;

void * worker(void *arg) {
    int i = 1e8;
    while (i--) {
        while(__sync_lock_test_and_set(&lock, 1) == 1);

        counter++;
        __sync_lock_release(&lock);
    }
}

void * worker2(void *arg) {
    int i = 1e8;
    while (i--) {
        pthread_mutex_lock(&mutex);
        counter++;
        pthread_mutex_unlock(&mutex);
    }
}

int main() {
    pthread_t t1, t2;
    struct timespec startMonotonic, endMonotonic;
    struct timespec startProcess, endProcess;
    pthread_mutex_init(&mutex, NULL);
    clock_gettime(CLOCK_MONOTONIC, &startMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProcess);
    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    clock_gettime(CLOCK_MONOTONIC, &endMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProcess);
    double time_takenMonotonic = (endMonotonic.tv_sec - startMonotonic.tv_sec) + (endMonotonic.tv_nsec - startMonotonic.tv_nsec) / 1e9;
    double time_takenProcess = (endProcess.tv_sec - startProcess.tv_sec) + (endProcess.tv_nsec - startProcess.tv_nsec) / 1e9;
    printf("Time taken (monotonic): %f seconds\n", time_takenMonotonic);
    printf("Time taken (process): %f seconds\n", time_takenProcess);

    printf("Counter: %d\n", counter);

    counter = 0;
    clock_gettime(CLOCK_MONOTONIC, &startMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProcess);
    pthread_create(&t1, NULL, worker2, NULL);
    pthread_create(&t2, NULL, worker2, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    clock_gettime(CLOCK_MONOTONIC, &endMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProcess);

    time_takenMonotonic = (endMonotonic.tv_sec - startMonotonic.tv_sec) + (endMonotonic.tv_nsec - startMonotonic.tv_nsec) / 1e9;
    time_takenProcess = (endProcess.tv_sec - startProcess.tv_sec) + (endProcess.tv_nsec - startProcess.tv_nsec) / 1e9;
    printf("Time taken (monotonic): %f seconds\n", time_takenMonotonic);
    printf("Time taken (process): %f seconds\n", time_takenProcess);
    printf("Counter: %d\n", counter);
    pthread_mutex_destroy(&mutex);
    return 0;
}