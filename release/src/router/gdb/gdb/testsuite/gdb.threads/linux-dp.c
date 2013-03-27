/* linux-dp.c --- dining philosophers, on LinuxThreads
   Jim Blandy <jimb@cygnus.com> --- March 1999  */

/* It's okay to edit this file and shift line numbers around.  The
   tests use gdb_get_line_number to find source locations, so they
   don't depend on having certain line numbers in certain places.  */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>

/* The number of philosophers at the table.  */
int num_philosophers;

/* Mutex ordering -
   If you want to lock a mutex M, all the mutexes you have locked
   already must appear before M on this list.

   fork_mutex[0]
   fork_mutex[1]
   ...
   fork_mutex[num_philosophers - 1]
   stdout_mutex
   random_mutex
*/

/* You must hold this mutex while writing to stdout.  */
pthread_mutex_t stdout_mutex;

/* You must hold this mutex while calling any of the random number
   generation routines.  */
pthread_mutex_t random_mutex;

/* array of mutexes, one for each fork; fork_mutex[i] is to the left
   of philosopher i.  A philosopher is holding fork i iff his/her
   thread has locked fork_mutex[i].  */
pthread_mutex_t *fork_mutex;

/* array of threads, one representing each philosopher.  */
pthread_t *philosophers;

void *
xmalloc (size_t n)
{
  void *p = malloc (n);

  if (! p)
    {
      fprintf (stderr, "out of memory\n");
      exit (2);
    }

  return p;
}

void
shared_printf (char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  pthread_mutex_lock (&stdout_mutex);
  vprintf (format, ap);
  pthread_mutex_unlock (&stdout_mutex);
  va_end (ap);
}

int 
shared_random ()
{
  static unsigned int seed;
  int result;

  pthread_mutex_lock (&random_mutex);
  result = rand_r (&seed);
  pthread_mutex_unlock (&random_mutex);
  return result;
}

void
my_usleep (long usecs)
{
  struct timeval timeout;
  
  timeout.tv_sec = usecs / 1000000;
  timeout.tv_usec = usecs % 1000000;

  select (0, 0, 0, 0, &timeout);
}

void
random_delay ()
{
  my_usleep ((shared_random () % 2000) * 100);
}

void
print_philosopher (int n, char left, char right)
{
  int i;

  shared_printf ("%*s%c %d %c\n", (n * 4) + 2, "", left, n, right);
}

void *
philosopher (void *data)
{
  int n = * (int *) data;

  print_philosopher (n, '_', '_');

#if 1
  if (n == num_philosophers - 1)
    for (;;)
      {
	/* The last philosopher is different.  He goes for his right
	   fork first, so there is no cycle in the mutex graph.  */

	/* Grab the right fork.  */
	pthread_mutex_lock (&fork_mutex[(n + 1) % num_philosophers]);
	print_philosopher (n, '_', '!');
	random_delay ();

	/* Then grab the left fork. */
	pthread_mutex_lock (&fork_mutex[n]);
	print_philosopher (n, '!', '!');
	random_delay ();

	print_philosopher (n, '_', '_');
	pthread_mutex_unlock (&fork_mutex[n]);
	pthread_mutex_unlock (&fork_mutex[(n + 1) % num_philosophers]);
	random_delay ();
      }
  else
#endif
    for (;;)
      {
	/* Grab the left fork. */
	pthread_mutex_lock (&fork_mutex[n]);
	print_philosopher (n, '!', '_');
	random_delay ();

	/* Then grab the right fork.  */
	pthread_mutex_lock (&fork_mutex[(n + 1) % num_philosophers]);
	print_philosopher (n, '!', '!');
	random_delay ();

	print_philosopher (n, '_', '_');
	pthread_mutex_unlock (&fork_mutex[n]);
	pthread_mutex_unlock (&fork_mutex[(n + 1) % num_philosophers]);
	random_delay ();
      }

  return (void *) 0;
}

int
main (int argc, char **argv)
{
  num_philosophers = 5;

  /* Set up the mutexes.  */
  {
    pthread_mutexattr_t ma;
    int i;

    pthread_mutexattr_init (&ma);
    pthread_mutex_init (&stdout_mutex, &ma);
    pthread_mutex_init (&random_mutex, &ma);
    fork_mutex = xmalloc (num_philosophers * sizeof (fork_mutex[0]));
    for (i = 0; i < num_philosophers; i++)
      pthread_mutex_init (&fork_mutex[i], &ma);
    pthread_mutexattr_destroy (&ma);
  }

  /* Set off the threads.  */
  {
    int i;
    int *numbers = xmalloc (num_philosophers * sizeof (*numbers));
    pthread_attr_t ta;

    philosophers = xmalloc (num_philosophers * sizeof (*philosophers));

    pthread_attr_init (&ta);
    
    for (i = 0; i < num_philosophers; i++)
      {
	numbers[i] = i;
	/* linuxthreads.exp: create philosopher */
	pthread_create (&philosophers[i], &ta, philosopher, &numbers[i]);
      }
    
    pthread_attr_destroy (&ta);
  }

  /* linuxthreads.exp: info threads 2 */
  sleep (1000000);

  /* Drink yourself into oblivion.  */
  for (;;)
    sleep (1000000);

  return 0;
}
