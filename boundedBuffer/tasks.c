#include "tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

void* task_1(void* arg) {
    (void)arg;
    volatile long x = 0;
    for (int i = 0; i < 100; i++)
        x += i * 2 + 1;
    (void)x;
    return NULL;
}

static void busy_ms(long n) {
    struct timespec end, now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    end.tv_sec = now.tv_sec + n / 1000;
    end.tv_nsec = now.tv_nsec + (n % 1000) * 1000000;
    if (end.tv_nsec >= 1000000000) {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
    }
    volatile long x = 0;
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > end.tv_sec ||
            (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
            break;
        for (int i = 0; i < 1000; i++)
            x += i * 3 - 1;
    }
    (void)x;
}

void* task_2(void* arg) {
    (void)arg;
    busy_ms(80);
    return NULL;
}

void* task_3(void* arg) {
    (void)arg;
    usleep(10000);
    return NULL;
}

void* task_4(void* arg) {
    (void)arg;
    char path[] = "/tmp/bb_task_XXXXXX";
    int fd = mkstemp(path);
    if (fd == -1)
        return NULL;
    const char* msg = "bounded buffer file task\n";
    size_t len = strlen(msg);
    if (write(fd, msg, len) != (ssize_t)len) {
        close(fd);
        unlink(path);
        return NULL;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        close(fd);
        unlink(path);
        return NULL;
    }
    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    unlink(path);
    if (n > 0)
        buf[n] = '\0';
    return NULL;
}
