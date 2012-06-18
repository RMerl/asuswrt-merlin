#ifndef __BIN_SEM_ASUS_H
#define __BIN_SEM_ASUS_H

/* our shared key (syslogd.c and logread.c must be in sync) */
enum { KEY_ID = 0x53555341 }; /* "ASUS" */

/* We must define union semun ourselves. */
union semun
{
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};

int bin_sem_alloc(key_t key, int sem_flags);
int bin_sem_dealloc(int semid);
int bin_sem_init();
int bin_sem_wait();
int bin_sem_post();

#endif
