#ifndef FORKIT_H
#define FORKIT_H

#ifndef OS2
#include <poll.h>
#endif

#include "port.h"

class Fork;

typedef void *PVOID;

typedef void(* FUNCTION)(Fork *, PVOID, int);

typedef struct
{
  FILE_TYPE child_read, child_write;
  Fork *f;
  PVOID param;
  FUNCTION func;
  int threadNum;
} THREAD_DATA;

class Fork
{
public:
  Fork();
  void go(FUNCTION func, PVOID param, int num);


  int Read(PVOID buf, int size, int timeout = 60);
  int Write(PVOID buf, int size, int timeout = 60);

  void startit(THREAD_DATA *td); // free td when finished

  int wait();

  int getNumThreads() const { return m_numThreads; }

private:
#ifndef OS2
  pollfd m_readPoll;
  pollfd m_writePoll;
#endif
  FILE_TYPE m_read;
  FILE_TYPE m_write;
  int m_numThreads;
};

#endif

