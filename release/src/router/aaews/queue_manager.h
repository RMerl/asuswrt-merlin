#ifndef _QUEUE_MANAGER_H
#define _QUEUE_MANAGER_H
#include <pthread.h>
#include <queue_list.h>
int InitQueue(pthread_mutex_t* lock, QUEUE* q );
int QueueIsEmpty(pthread_mutex_t* lock, QUEUE* q);
int QueueLength(pthread_mutex_t* lock, QUEUE* q);
int PushQueue(pthread_mutex_t* lock, QUEUE* q, void* y);
void* PopQueue(pthread_mutex_t* lock, QUEUE* q);
#endif
