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
typedef struct {
    volatile int ticket;
    volatile int turn;
} ticketlock_t;

ticketlock_t ticketlock = {0, 0};

volatile int testAndSetLock = 0;
void lock_fetchAndAdd(){
    int my_ticket = __sync_fetch_and_add(&ticketlock.ticket, 1);
    while(ticketlock.turn != my_ticket);
}

void unlock_fetchAndAdd(){
    __sync_add_and_fetch(&ticketlock.turn, 1);
}

void* worker_fetchAndAdd(void * arg){
    for(int i = 0; i < 1e8; i++){
        lock_fetchAndAdd();
        counter++;
        unlock_fetchAndAdd();
    }
    return NULL;
}

void lock_testAndSet(){
    while(__sync_lock_test_and_set(&testAndSetLock, 1) == 1);
}

void unlock_testAndSet(){
    __sync_lock_release(&testAndSetLock);
}

void* worker_testAndSet(void * arg){
    int *id = (int*)arg;
    for(int i = 0; i < 1e8; i++){
        lock_testAndSet();
        counter++;
        unlock_testAndSet();
    }
    if (id != NULL)
        printf("Thread %d finished\n", *id);
    return NULL;
}
int main(){
    pthread_t threads[10];
    int num_threads = 4;
    int thread_ids[10];

    struct timespec startMonotonic, startProcess;
    struct timespec endMonotonic, endProcess;
    for (int i = 0; i < num_threads; i++)
        thread_ids[i] = i;

    clock_gettime(CLOCK_MONOTONIC, &startMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProcess);
    for(int i = 0; i < num_threads; i++){
        pthread_create(&threads[i], NULL, worker_fetchAndAdd, (void*)&thread_ids[i]);
    }
    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &endMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProcess);
    double time_takenMonotonic = (endMonotonic.tv_sec - startMonotonic.tv_sec) + (endMonotonic.tv_nsec - startMonotonic.tv_nsec) / 1e9;
    double time_takenProcess = (endProcess.tv_sec - startProcess.tv_sec) + (endProcess.tv_nsec - startProcess.tv_nsec) / 1e9;
    printf("fetchAndAdd - Time taken (monotonic): %f seconds\n", time_takenMonotonic);
    printf("fetchAndAdd - Time taken (process): %f seconds\n", time_takenProcess);
    printf("fetchAndAdd - Counter: %d\n", counter);

    counter = 0;
    clock_gettime(CLOCK_MONOTONIC, &startMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startProcess);
    for(int i = 0; i < num_threads; i++){
        pthread_create(&threads[i], NULL, worker_testAndSet, (void*)&thread_ids[i]);
    }
    for(int i = 0; i < num_threads; i++){
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &endMonotonic);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endProcess);
    time_takenMonotonic = (endMonotonic.tv_sec - startMonotonic.tv_sec) + (endMonotonic.tv_nsec - startMonotonic.tv_nsec) / 1e9;
    time_takenProcess = (endProcess.tv_sec - startProcess.tv_sec) + (endProcess.tv_nsec - startProcess.tv_nsec) / 1e9;
    printf("testAndSet - Time taken (monotonic): %f seconds\n", time_takenMonotonic);
    printf("testAndSet - Time taken (process): %f seconds\n", time_takenProcess);
    printf("testAndSet - Counter: %d\n", counter);
    return 0;
}