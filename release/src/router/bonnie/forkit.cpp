
#ifdef OS2
#define INCL_DOSPROCESS
#else
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "forkit.h"


Fork::Fork()
 : m_read(-1)
 , m_write(-1)
 , m_numThreads(0)
{
}

#ifdef OS2
// for the benefit of this function and the new Fork class it may create
// the Fork class must do nothing of note in it's constructor or it's
// go() member function.
VOID APIENTRY thread_func(ULONG param)
{
  THREAD_DATA *td = (THREAD_DATA *)param;
  td->f = new Fork;
  td->f->startit(td);
}
#endif

void Fork::startit(THREAD_DATA *td)
{
  m_read = td->child_read;
  m_write = td->child_write;
  td->func(this, td->param, td->threadNum);
  delete td->f; // delete ourself if td->f is not NULL
  delete td;
  exit(0);
}

void Fork::go(FUNCTION func, PVOID param, int num)
{
  m_numThreads = num;
  FILE_TYPE control[2];
  FILE_TYPE feedback[2];
  if (pipe(feedback) || pipe(control))
  {
    fprintf(stderr, "Can't open pipes.\n");
    exit(1);
  }
#ifndef OS2
  m_readPoll.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
  m_writePoll.events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
#endif

  THREAD_DATA *td = new THREAD_DATA;
  td->child_read = control[0];
  td->child_write = feedback[1];
  td->f = NULL;
  td->param = param;
  td->func = func;
  for(int i = 0; i < num; i++)
  {
#ifdef OS2
    THREAD_DATA *tmp = new THREAD_DATA;
    memcpy(tmp, td, sizeof(THREAD_DATA));
#endif
    td->threadNum = i;
#ifdef OS2
    // yes I know I am casting a pointer to an unsigned long
    // it's the way you're supposed to do things in OS/2
    TID id = 0;
    if(DosCreateThread(&id, thread_func, ULONG(td), CREATE_READY, 32*1024))
    {
      fprintf(stderr, "Can't create a thread.\n");
      exit(1);
    }
#else
    int p = fork();
    if(p == -1)
    {
      fprintf(stderr, "Can't fork.\n");
      exit(1);
    }
    if(p == 0) // child
    {
      m_readPoll.fd = td->child_read;
      m_writePoll.fd = td->child_write;
      file_close(control[1]);
      file_close(feedback[0]);
      srand(getpid() ^ time(NULL));
      startit(td);
    }
#endif
  }
  // now we're in the parent thread/process
  m_write = control[1];
  m_read = feedback[0];
#ifndef OS2
  m_readPoll.fd = m_read;
  m_writePoll.fd = m_write;
  file_close(control[0]);
  file_close(feedback[1]);
#endif
}

int Fork::wait()
{
#ifdef OS2
  TID status = 0;
  if(DosWaitThread(&status, DCWW_WAIT))
  {
    fprintf(stderr, "Can't wait for thread.\n");
    return 1;
  }
#else
  int status = 0;
  if(::wait(&status) == -1)
  {
    fprintf(stderr, "Can't wait for thread.\n");
    return 1;
  }
#endif
  return 0;
}

int Fork::Read(PVOID buf, int size, int timeout)
{
#ifndef OS2
  if(timeout)
  {
    int rc = poll(&m_readPoll, 1, timeout * 1000);
    if(rc < 0)
    {
      fprintf(stderr, "Can't poll.\n");
      return -1;
    }
    if(!rc)
      return 0;
  }
#endif
#ifdef OS2
  unsigned long actual;
  int rc = DosRead(m_read, buf, size, &actual);
  if(rc || actual != size)
#else
  if(size != read(m_read, buf, size) )
#endif
  {
    fprintf(stderr, "Can't read data from IPC pipe.\n");
    return -1;
  }
  return size;
}

int Fork::Write(PVOID buf, int size, int timeout)
{
#ifndef OS2
  if(timeout)
  {
    int rc = poll(&m_writePoll, 1, timeout * 1000);
    if(rc < 0)
    {
      fprintf(stderr, "Can't poll for write.\n");
      return -1;
    }
    if(!rc)
      return 0;
  }
#endif
#ifdef OS2
  unsigned long actual;
  int rc = DosWrite(m_write, buf, size, &actual);
  if(rc || actual != size)
#else
  if(size != write(m_write, buf, size))
#endif
  {
    fprintf(stderr, "Can't write data to IPC pipe.\n");
    return -1;
  }
  return size;
}

