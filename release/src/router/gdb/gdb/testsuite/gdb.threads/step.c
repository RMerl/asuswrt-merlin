/*    step.c for step.exp    */
#include <ipc.h>
#include <pthread.h>
#include <st.h>
#include <signal.h>
#include <stdio.h>

void alarm_handler ();
void alarm_handler1 ();
void alarm_handler2 ();
void thread1 ();
void thread2 ();

#define TIME_LIMIT 30


int count1 = 0;
int count2 = 0;

pthread_t tid1, tid2;
pthread_attr_t attr1, attr2;

pthread_mutex_t mut;
pthread_mutexattr_t mut_attr;

pthread_condattr_t cv_attr_a, cv_attr_b;
pthread_cond_t cv_a, cv_b;

struct cv_struct
  {
    char a;
    char b;
  }
test_struct;

main ()
{
  /*init la struct */
  test_struct.a = 0;
  test_struct.b = 1;

  /* create le mutex */
  if (pthread_mutexattr_create (&mut_attr) == -1)
    {
      perror ("mutexattr_create");
      exit (1);
    }


  if (pthread_mutex_init (&mut, mut_attr) == -1)
    {
      perror ("mutex_init");
      exit (1);
    }

  /* create 2 cv */
  if (pthread_condattr_create (&cv_attr_a) == -1)
    {
      perror ("condattr_create(1)");
      exit (1);
    }

  if (pthread_cond_init (&cv_a, cv_attr_a) == -1)
    {
      perror ("cond_init(1)");
      exit (1);
    }

  if (pthread_condattr_create (&cv_attr_b) == -1)
    {
      perror ("condattr_create(2)");
      exit (1);
    }

  if (pthread_cond_init (&cv_b, cv_attr_b) == -1)
    {
      perror ("cond_init(2)");
      exit (1);
    }

  /* create 2 threads of execution */
  if (pthread_attr_create (&attr1) == -1)
    {
      perror ("attr_create(1)");
      exit (1);
    }

  if (pthread_create (&tid1, attr1, thread1, &count1) == -1)
    {
      perror ("pthread_create(1)");
      exit (1);
    }

  if (pthread_attr_create (&attr2) == -1)
    {
      perror ("attr_create(2)");
      exit (1);
    }

  if (pthread_create (&tid2, attr2, thread2, &count2) == -1)
    {
      perror ("pthread_create(2)");
      exit (1);
    }

  /* set alarm to print out data and exit */
  signal (SIGALRM, alarm_handler);
  alarm (TIME_LIMIT);

  for (;;)
    pause ();
}

void 
thread1 (count)
     int *count;
{
  tid_t tid;

  tid = getstid ();
  printf ("Thread1 tid 0x%x  (%d) \n", tid, tid);
  printf ("Thread1 @tid=0x%x \n", &tid);
  signal (SIGALRM, alarm_handler1);

  for (;;)
    {
      if (pthread_mutex_lock (&mut) == -1)
	{
	  perror ("pthread_mutex_lock(1)");
	  pthread_exit ((void *) 0);
	}

      while (test_struct.a == 0)
	{
	  if (pthread_cond_wait (&cv_a, &mut) == -1)
	    {
	      perror ("pthread_cond_wait(1)");
	      pthread_exit ((void *) -1);
	    }
	}

      (*count)++;
      printf ("*******thread1 count %d\n", *count);

      test_struct.a = 0;

      test_struct.b = 1;
      pthread_cond_signal (&cv_b);

      if (pthread_mutex_unlock (&mut) == -1)
	{
	  perror ("pthread_mutex_unlock(1)");
	  pthread_exit ((void *) -1);
	}
    }
}

void 
thread2 (count)
     int *count;
{
  tid_t tid;

  tid = getstid ();
  printf ("Thread2 tid 0x%x  (%d) \n", tid, tid);
  printf ("Thread1 @tid=0x%x \n", &tid);
  signal (SIGALRM, alarm_handler2);

  for (;;)
    {
      if (pthread_mutex_lock (&mut) == -1)
	{
	  perror ("pthread_mutex_lock(2)");
	  pthread_exit ((void *) 0);
	}

      while (test_struct.b == 0)
	{
	  if (pthread_cond_wait (&cv_b, &mut) == -1)
	    {
	      perror ("pthread_cond_wait(2)");
	      pthread_exit ((void *) -1);
	    }
	}

      (*count)++;
      printf ("*******thread2 count %d\n", *count);

      test_struct.b = 0;

      test_struct.a = 1;
      pthread_cond_signal (&cv_a);

      if (pthread_mutex_unlock (&mut) == -1)
	{
	  perror ("pthread_mutex_unlock(2)");
	  pthread_exit ((void *) -1);
	}
    }
}


void 
alarm_handler ()
{
  printf ("\tcount1 (%d) \n\tcount2 (%d)\n", count1, count2);
  exit (0);
}

void 
alarm_handler1 ()
{
  printf ("ALARM thread 1\n");
}

void 
alarm_handler2 ()
{
  printf ("ALARM thread 2\n");
  pthread_exit ((void *) 0);
}
