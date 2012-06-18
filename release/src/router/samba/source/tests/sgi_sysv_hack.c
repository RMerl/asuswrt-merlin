/* this tests if we need to define SGI_SEMUN_HACK
   if we're using gcc on IRIX 6.5.x. */

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#ifndef HAVE_UNION_SEMUN
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif

union semun_hack {
 int val;
 struct semid_ds *buf;
 unsigned short *array;
 char __dummy[5];
};

main() {
  struct semid_ds sem_ds;
  union semun_hack suh;
  union semun su;
  int sem_id, ret;

  ret = 1;
  sem_id = semget(0xdead6666,1,IPC_CREAT|IPC_EXCL|0777);
  su.buf = &sem_ds;
  suh.buf = &sem_ds;
  if (sem_id != -1) {
    if ((semctl(sem_id, 0, IPC_STAT, su) == -1) &&
        (semctl(sem_id, 0, IPC_STAT, suh) != -1)) {
      ret = 0;
    }
  }
  semctl(sem_id, 0, IPC_RMID, 0);
  return ret;
}
