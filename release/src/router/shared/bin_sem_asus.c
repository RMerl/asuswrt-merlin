#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include "bin_sem_asus.h"

/* ipc semaphore id */
int semid_bs = -1;

/* Obtain a binary semaphore's ID, allocating if necessary. */
int bin_sem_alloc(key_t key, int sem_flags)
{
	return semget (key, 1, sem_flags);
}

/* Deallocate a binary semaphore. All users must have finished their 
   use. Returns -1 on failure. */
int bin_sem_dealloc(int semid)
{
	union semun ignored_argument;

	return semctl (semid, 1, IPC_RMID, ignored_argument);
}

/* Initialize a binary semaphore with a value of 1. */
int bin_sem_init()
{
	union semun argument;
	unsigned short values[1];
	values[0] = 1;
	argument.array = values;

	semid_bs = bin_sem_alloc(KEY_ID, IPC_CREAT | IPC_EXCL | 1023);

	if (semid_bs == -1)
	{
		if (errno == EEXIST)
		{
			semid_bs = bin_sem_alloc(KEY_ID, 0);
			if (semid_bs == -1)
				return -1;
		}
		else
			return -1;
	}

	return semctl (semid_bs, 0, SETALL, argument);
}

/* Wait on a binary semaphore. Block until the semaphore value is positive, then
   decrement it by 1. */
int bin_sem_wait()
{
	struct sembuf operations[1];
	/* Use the first (and only) semaphore. */
	operations[0].sem_num = 0;
	/* Decrement by 1. */
	operations[0].sem_op = -1;
	/* Permit undo'ing. */
	operations[0].sem_flg = SEM_UNDO;

	return semop (semid_bs, operations, 1);
}

/* Post to a binary semaphore: increment its value by 1.
   This returns immediately. */
int bin_sem_post()
{
	struct sembuf operations[1];
	/* Use the first (and only) semaphore. */ 
	operations[0].sem_num = 0;
	/* Increment by 1. */ 
	operations[0].sem_op = 1;
	/* Permit undo'ing. */ 
	operations[0].sem_flg = SEM_UNDO;

	return semop (semid_bs, operations, 1);
}
