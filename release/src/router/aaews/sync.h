#ifndef _SYNC_H
#define _SYNC_H
#include <semaphore.h>
#if 0
int init_event(sem_t* sem);
int wait_event(sem_t* sem);
int set_event(sem_t* sem);
#endif
int init_event(sem_t* sem);
int set_waiting_event( sem_t* sem);
//int set_event_alert(sem_t* sem);
//int wait_event(pthread_t tid, void** res );
int wait_event(sem_t* se);
int set_event(sem_t* se);
int init_semaphore(sem_t** sem, const char* name);
int deinit_semaphore(sem_t* sem, const char* name);
#endif
