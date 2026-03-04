#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
gcc -o main main.c threadPool_mutex_cv.c consumer.c tasks.c -lpthread
./main 16