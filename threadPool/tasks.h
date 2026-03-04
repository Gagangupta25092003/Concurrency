#ifndef TASKS_H
#define TASKS_H

/* Task 1: Small (fast, minimal work)
 * Task 2: Heavy (CPU-bound)
 * Task 3: Sleeping (blocks briefly)
 * Task 4: File read/write
 */
void* task_1(void* arg);
void* task_2(void* arg);
void* task_3(void* arg);
void* task_4(void* arg);

#endif /* TASKS_H */
