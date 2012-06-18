/* this tests whether we can use a sysv shared memory segment
   as needed for the sysv varient of FAST_SHARE_MODES */

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define KEY 0x963796
#define SEMKEY 0x963797
#define SIZE (32*1024)

#ifndef HAVE_UNION_SEMUN
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif


main()
{
	int id, sem_id;
	int *buf;
	int count=7;
	union semun su;

#ifdef LINUX
	if (sizeof(struct shmid_ds) == 52) {
		printf("WARNING: You probably have a broken set of glibc2 include files - disabling sysv shared memory\n");
		exit(1);
	}
#endif


	sem_id = semget(SEMKEY, 1, IPC_CREAT|IPC_EXCL|0600);

	if (sem_id == -1) exit(1);

	su.val = 1;
	semctl(sem_id, 0, IPC_RMID, su);

	id = shmget(KEY, 0, 0);
	if (id != -1) {
		if (shmctl(id, IPC_RMID, 0) != 0) exit(1);
	}

	if (fork() == 0) {
		/* uggh - need locking */
		sleep(2);

		/* get an existing area */
		id = shmget(KEY, 0, 0);
		if (id == -1) exit(1);

		buf = (int *)shmat(id, 0, 0);
		if (buf == (int *)-1) exit(1);


		while (count-- && buf[6124] != 55732) sleep(1);

		if (count <= 0) exit(1);

		buf[1763] = 7268;
		exit(0);
	}
	
	id = shmget(KEY, SIZE, IPC_CREAT | IPC_EXCL | 0600);
	if (id == -1) exit(1);

	buf = (int *)shmat(id, 0, 0);

	if (buf == (int *)-1) exit(1);

	buf[6124] = 55732;

	while (count-- && buf[1763] != 7268) sleep(1);

	shmctl(id, IPC_RMID, 0);

	if (count <= 0) exit(1);
	exit(0);
}
