#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#ifndef SEMUN_IN_SEM_H
union semun
{
  int val;                    /* value for SETVAL */
  struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
  unsigned short int *array;  /* array for GETALL, SETALL */
  struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif

class Semaphore
{
public:

  // numSems is the number of semaphores to be in the set
  // semKey is the ID number for the semaphore set
  // val is the initial value for the semaphores, no values will be assigned
  // if the default (0) is specified.
  Semaphore(int semKey, int numSems = 1, int val = 0);

  // clear the semaphores and return an error code.
  int clear_sem();

  // create the semaphores
  // count is the initial value assigned to each semaphore
  int create(int count);

  // get the handle to a semaphore set previously created
  int get_semid();

  int decrement_and_wait(int nr_sem);
  int get_mutex();
  int put_mutex();

private:
  union semun m_arg;
  int m_semid;
  int m_semflg;
  bool m_semopen;
  int m_semKey;
  int m_numSems;
};

#endif

