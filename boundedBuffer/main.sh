#!/bin/bash
gcc -o main main.c boundedBuffer_mutex_cv.c producer_consumer.c tasks.c task.c -lpthread
./main 16
