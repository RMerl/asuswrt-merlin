/* Thread management routine
 * Copyright (C) 1998, 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* #define DEBUG */

#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "log.h"

/* Struct timeval's tv_usec one second value.  */
#define TIMER_SECOND_MICRO 1000000L

struct timeval
timeval_adjust (struct timeval a)
{
  while (a.tv_usec >= TIMER_SECOND_MICRO)
    {
      a.tv_usec -= TIMER_SECOND_MICRO;
      a.tv_sec++;
    }

  while (a.tv_usec < 0)
    {
      a.tv_usec += TIMER_SECOND_MICRO;
      a.tv_sec--;
    }

  if (a.tv_sec < 0)
    {
      a.tv_sec = 0;
      a.tv_usec = 10;
    }

  if (a.tv_sec > TIMER_SECOND_MICRO)
    a.tv_sec = TIMER_SECOND_MICRO;    

  return a;
}

static struct timeval
timeval_subtract (struct timeval a, struct timeval b)
{
  struct timeval ret;

  ret.tv_usec = a.tv_usec - b.tv_usec;
  ret.tv_sec = a.tv_sec - b.tv_sec;

  return timeval_adjust (ret);
}

static int
timeval_cmp (struct timeval a, struct timeval b)
{
  return (a.tv_sec == b.tv_sec
	  ? a.tv_usec - b.tv_usec : a.tv_sec - b.tv_sec);
}

static unsigned long
timeval_elapsed (struct timeval a, struct timeval b)
{
  return (((a.tv_sec - b.tv_sec) * TIMER_SECOND_MICRO)
	  + (a.tv_usec - b.tv_usec));
}

/* List allocation and head/tail print out. */
static void
thread_list_debug (struct thread_list *list)
{
  printf ("count [%d] head [%p] tail [%p]\n",
	  list->count, list->head, list->tail);
}

/* Debug print for thread_master. */
void
thread_master_debug (struct thread_master *m)
{
  printf ("-----------\n");
  printf ("readlist  : ");
  thread_list_debug (&m->read);
  printf ("writelist : ");
  thread_list_debug (&m->write);
  printf ("timerlist : ");
  thread_list_debug (&m->timer);
  printf ("eventlist : ");
  thread_list_debug (&m->event);
  printf ("unuselist : ");
  thread_list_debug (&m->unuse);
  printf ("total alloc: [%ld]\n", m->alloc);
  printf ("-----------\n");
}

/* Allocate new thread master.  */
struct thread_master *
thread_master_create ()
{
  return (struct thread_master *) XCALLOC (MTYPE_THREAD_MASTER,
					   sizeof (struct thread_master));
}

/* Add a new thread to the list.  */
static void
thread_list_add (struct thread_list *list, struct thread *thread)
{
  thread->next = NULL;
  thread->prev = list->tail;
  if (list->tail)
    list->tail->next = thread;
  else
    list->head = thread;
  list->tail = thread;
  list->count++;
}

/* Add a new thread just before the point.  */
static void
thread_list_add_before (struct thread_list *list, 
			struct thread *point, 
			struct thread *thread)
{
  thread->next = point;
  thread->prev = point->prev;
  if (point->prev)
    point->prev->next = thread;
  else
    list->head = thread;
  point->prev = thread;
  list->count++;
}

/* Delete a thread from the list. */
static struct thread *
thread_list_delete (struct thread_list *list, struct thread *thread)
{
  if (thread->next)
    thread->next->prev = thread->prev;
  else
    list->tail = thread->prev;
  if (thread->prev)
    thread->prev->next = thread->next;
  else
    list->head = thread->next;
  thread->next = thread->prev = NULL;
  list->count--;
  return thread;
}

/* Move thread to unuse list. */
static void
thread_add_unuse (struct thread_master *m, struct thread *thread)
{
  assert (m != NULL);
  assert (thread->next == NULL);
  assert (thread->prev == NULL);
  assert (thread->type == THREAD_UNUSED);
  thread_list_add (&m->unuse, thread);
}

/* Free all unused thread. */
static void
thread_list_free (struct thread_master *m, struct thread_list *list)
{
  struct thread *t;
  struct thread *next;

  for (t = list->head; t; t = next)
    {
      next = t->next;
      XFREE (MTYPE_THREAD, t);
      list->count--;
      m->alloc--;
    }
}

/* Stop thread scheduler. */
void
thread_master_free (struct thread_master *m)
{
  thread_list_free (m, &m->read);
  thread_list_free (m, &m->write);
  thread_list_free (m, &m->timer);
  thread_list_free (m, &m->event);
  thread_list_free (m, &m->ready);
  thread_list_free (m, &m->unuse);

  XFREE (MTYPE_THREAD_MASTER, m);
}

/* Delete top of the list and return it. */
static struct thread *
thread_trim_head (struct thread_list *list)
{
  if (list->head)
    return thread_list_delete (list, list->head);
  return NULL;
}

/* Thread list is empty or not.  */
int
thread_empty (struct thread_list *list)
{
  return  list->head ? 0 : 1;
}

/* Return remain time. */
char *
thread_timer_remain_second (struct thread *thread)
{
  struct timeval timer_now;
  struct tm *tm;
  time_t remain_time;
  char buf[25];
  int len = 25;

  gettimeofday (&timer_now, NULL);

  remain_time = thread->u.sands.tv_sec - timer_now.tv_sec;

  if (remain_time < 0)
    remain_time = 0;

  tm = gmtime (&remain_time);

  /* Making formatted timer strings. */
#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

  if (remain_time < ONE_DAY_SECOND)
    snprintf (buf, len, "%02d:%02d:%02d",
              tm->tm_hour, tm->tm_min, tm->tm_sec);
  else if (remain_time < ONE_WEEK_SECOND)
    snprintf (buf, len, "%dd%02dh%02dm",
              tm->tm_yday, tm->tm_hour, tm->tm_min);
  else
    snprintf (buf, len, "%02dw%dd%02dh",
              tm->tm_yday/7, tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
  return buf;
}

/* Get new thread.  */
static struct thread *
thread_get (struct thread_master *m, u_char type,
	    int (*func) (struct thread *), void *arg)
{
  struct thread *thread;

  if (m->unuse.head)
    thread = thread_trim_head (&m->unuse);
  else
    {
      thread = XCALLOC (MTYPE_THREAD, sizeof (struct thread));
      m->alloc++;
    }
  thread->type = type;
  thread->master = m;
  thread->func = func;
  thread->arg = arg;
  
  return thread;
}

/* Add new read thread. */
struct thread *
thread_add_read (struct thread_master *m, 
		 int (*func) (struct thread *), void *arg, int fd)
{
  struct thread *thread;

  assert (m != NULL);

  if (FD_ISSET (fd, &m->readfd))
    {
      zlog (NULL, LOG_WARNING, "There is already read fd [%d]", fd);
      return NULL;
    }

  thread = thread_get (m, THREAD_READ, func, arg);
  FD_SET (fd, &m->readfd);
  thread->u.fd = fd;
  thread_list_add (&m->read, thread);

  return thread;
}

/* Add new write thread. */
struct thread *
thread_add_write (struct thread_master *m,
		 int (*func) (struct thread *), void *arg, int fd)
{
  struct thread *thread;

  assert (m != NULL);

  if (FD_ISSET (fd, &m->writefd))
    {
      zlog (NULL, LOG_WARNING, "There is already write fd [%d]", fd);
      return NULL;
    }

  thread = thread_get (m, THREAD_WRITE, func, arg);
  FD_SET (fd, &m->writefd);
  thread->u.fd = fd;
  thread_list_add (&m->write, thread);

  return thread;
}

/* Add timer event thread. */
struct thread *
thread_add_timer (struct thread_master *m,
		  int (*func) (struct thread *), void *arg, long timer)
{
  struct timeval timer_now;
  struct thread *thread;
#ifndef TIMER_NO_SORT
  struct thread *tt;
#endif /* TIMER_NO_SORT */

  assert (m != NULL);

  thread = thread_get (m, THREAD_TIMER, func, arg);

  /* Do we need jitter here? */
  gettimeofday (&timer_now, NULL);
  timer_now.tv_sec += timer;
  thread->u.sands = timer_now;

  /* Sort by timeval. */
#ifdef TIMER_NO_SORT
  thread_list_add (&m->timer, thread);
#else
  for (tt = m->timer.head; tt; tt = tt->next)
    if (timeval_cmp (thread->u.sands, tt->u.sands) <= 0)
      break;

  if (tt)
    thread_list_add_before (&m->timer, tt, thread);
  else
    thread_list_add (&m->timer, thread);
#endif /* TIMER_NO_SORT */

  return thread;
}

/* Add simple event thread. */
struct thread *
thread_add_event (struct thread_master *m,
		  int (*func) (struct thread *), void *arg, int val)
{
  struct thread *thread;

  assert (m != NULL);

  thread = thread_get (m, THREAD_EVENT, func, arg);
  thread->u.val = val;
  thread_list_add (&m->event, thread);

  return thread;
}

/* Cancel thread from scheduler. */
void
thread_cancel (struct thread *thread)
{
  switch (thread->type)
    {
    case THREAD_READ:
      assert (FD_ISSET (thread->u.fd, &thread->master->readfd));
      FD_CLR (thread->u.fd, &thread->master->readfd);
      thread_list_delete (&thread->master->read, thread);
      break;
    case THREAD_WRITE:
      assert (FD_ISSET (thread->u.fd, &thread->master->writefd));
      FD_CLR (thread->u.fd, &thread->master->writefd);
      thread_list_delete (&thread->master->write, thread);
      break;
    case THREAD_TIMER:
      thread_list_delete (&thread->master->timer, thread);
      break;
    case THREAD_EVENT:
      thread_list_delete (&thread->master->event, thread);
      break;
    case THREAD_READY:
      thread_list_delete (&thread->master->ready, thread);
      break;
    default:
      break;
    }
  thread->type = THREAD_UNUSED;
  thread_add_unuse (thread->master, thread);
}

/* Delete all events which has argument value arg. */
void
thread_cancel_event (struct thread_master *m, void *arg)
{
  struct thread *thread;

  thread = m->event.head;
  while (thread)
    {
      struct thread *t;

      t = thread;
      thread = t->next;

      if (t->arg == arg)
	{
	  thread_list_delete (&m->event, t);
	  t->type = THREAD_UNUSED;
	  thread_add_unuse (m, t);
	}
    }
}

#ifdef TIMER_NO_SORT
struct timeval *
thread_timer_wait (struct thread_master *m, struct timeval *timer_val)
{
  struct timeval timer_now;
  struct timeval timer_min;
  struct timeval *timer_wait;

  gettimeofday (&timer_now, NULL);

  timer_wait = NULL;
  for (thread = m->timer.head; thread; thread = thread->next)
    {
      if (! timer_wait)
	timer_wait = &thread->u.sands;
      else if (timeval_cmp (thread->u.sands, *timer_wait) < 0)
	timer_wait = &thread->u.sands;
    }

  if (m->timer.head)
    {
      timer_min = *timer_wait;
      timer_min = timeval_subtract (timer_min, timer_now);
      if (timer_min.tv_sec < 0)
	{
	  timer_min.tv_sec = 0;
	  timer_min.tv_usec = 10;
	}
      timer_wait = &timer_min;
    }
  else
    timer_wait = NULL;

  if (timer_wait)
    {
      *timer_val = timer_wait;
      return timer_val;
    }
  return NULL;
}
#else /* ! TIMER_NO_SORT */
struct timeval *
thread_timer_wait (struct thread_master *m, struct timeval *timer_val)
{
  struct timeval timer_now;
  struct timeval timer_min;

  if (m->timer.head)
    {
      gettimeofday (&timer_now, NULL);
      timer_min = m->timer.head->u.sands;
      timer_min = timeval_subtract (timer_min, timer_now);
      if (timer_min.tv_sec < 0)
	{
	  timer_min.tv_sec = 0;
	  timer_min.tv_usec = 10;
	}
      *timer_val = timer_min;
      return timer_val;
    }
  return NULL;
}
#endif /* TIMER_NO_SORT */

struct thread *
thread_run (struct thread_master *m, struct thread *thread,
	    struct thread *fetch)
{
  *fetch = *thread;
  thread->type = THREAD_UNUSED;
  thread_add_unuse (m, thread);
  return fetch;
}

int
thread_process_fd (struct thread_master *m, struct thread_list *list,
		   fd_set *fdset, fd_set *mfdset)
{
  struct thread *thread;
  struct thread *next;
  int ready = 0;

  for (thread = list->head; thread; thread = next)
    {
      next = thread->next;

      if (FD_ISSET (THREAD_FD (thread), fdset))
	{
	  assert (FD_ISSET (THREAD_FD (thread), mfdset));
	  FD_CLR(THREAD_FD (thread), mfdset);
	  thread_list_delete (list, thread);
	  thread_list_add (&m->ready, thread);
	  thread->type = THREAD_READY;
	  ready++;
	}
    }
  return ready;
}

/* Fetch next ready thread. */
struct thread *
thread_fetch (struct thread_master *m, struct thread *fetch)
{
  int num;
  int ready;
  struct thread *thread;
  fd_set readfd;
  fd_set writefd;
  fd_set exceptfd;
  struct timeval timer_now;
  struct timeval timer_val;
  struct timeval *timer_wait;
  struct timeval timer_nowait;

  timer_nowait.tv_sec = 0;
  timer_nowait.tv_usec = 0;

  while (1)
    {
      /* Normal event is the highest priority.  */
      if ((thread = thread_trim_head (&m->event)) != NULL)
	return thread_run (m, thread, fetch);

      /* Execute timer.  */
      gettimeofday (&timer_now, NULL);

      for (thread = m->timer.head; thread; thread = thread->next)
	if (timeval_cmp (timer_now, thread->u.sands) >= 0)
	  {
	    thread_list_delete (&m->timer, thread);
	    return thread_run (m, thread, fetch);
	  }

      /* If there are any ready threads, process top of them.  */
      if ((thread = thread_trim_head (&m->ready)) != NULL)
	return thread_run (m, thread, fetch);

      /* Structure copy.  */
      readfd = m->readfd;
      writefd = m->writefd;
      exceptfd = m->exceptfd;

      /* Calculate select wait timer. */
      timer_wait = thread_timer_wait (m, &timer_val);

      num = select (FD_SETSIZE, &readfd, &writefd, &exceptfd, timer_wait);

      if (num == 0)
	continue;

      if (num < 0)
	{
	  if (errno == EINTR)
	    continue;

	  zlog_warn ("select() error: %s", strerror (errno));
	  return NULL;
	}

      /* Normal priority read thead. */
      ready = thread_process_fd (m, &m->read, &readfd, &m->readfd);

      /* Write thead. */
      ready = thread_process_fd (m, &m->write, &writefd, &m->writefd);

      if ((thread = thread_trim_head (&m->ready)) != NULL)
	return thread_run (m, thread, fetch);
    }
}

static unsigned long
thread_consumed_time (RUSAGE_T *now, RUSAGE_T *start)
{
  unsigned long thread_time;

#ifdef HAVE_RUSAGE
  /* This is 'user + sys' time.  */
  thread_time = timeval_elapsed (now->ru_utime, start->ru_utime);
  thread_time += timeval_elapsed (now->ru_stime, start->ru_stime);
#else
  /* When rusage is not available, simple elapsed time is used.  */
  thread_time = timeval_elapsed (*now, *start);
#endif /* HAVE_RUSAGE */

  return thread_time;
}

/* We should aim to yield after THREAD_YIELD_TIME_SLOT
   milliseconds.  */
int
thread_should_yield (struct thread *thread)
{
  RUSAGE_T ru;

  GETRUSAGE (&ru);

  if (thread_consumed_time (&ru, &thread->ru) > THREAD_YIELD_TIME_SLOT)
    return 1;
  else
    return 0;
}

/* We check thread consumed time. If the system has getrusage, we'll
   use that to get indepth stats on the performance of the thread.  If
   not - we'll use gettimeofday for some guestimation.  */
void
thread_call (struct thread *thread)
{
  unsigned long thread_time;
  RUSAGE_T ru;

  GETRUSAGE (&thread->ru);

  (*thread->func) (thread);

  GETRUSAGE (&ru);

  thread_time = thread_consumed_time (&ru, &thread->ru);

#ifdef THREAD_CONSUMED_TIME_CHECK
  if (thread_time > 200000L)
    {
      /*
       * We have a CPU Hog on our hands.
       * Whinge about it now, so we're aware this is yet another task
       * to fix.
       */
      zlog_err ("CPU HOG task %lx ran for %ldms",
                /* FIXME: report the name of the function somehow */
		(unsigned long) thread->func,
		thread_time / 1000L);
    }
#endif /* THREAD_CONSUMED_TIME_CHECK */
}

/* Execute thread */
struct thread *
thread_execute (struct thread_master *m,
                int (*func)(struct thread *), 
                void *arg,
                int val)
{
  struct thread dummy; 

  memset (&dummy, 0, sizeof (struct thread));

  dummy.type = THREAD_EVENT;
  dummy.master = NULL;
  dummy.func = func;
  dummy.arg = arg;
  dummy.u.val = val;
  thread_call (&dummy);

  return NULL;
}
