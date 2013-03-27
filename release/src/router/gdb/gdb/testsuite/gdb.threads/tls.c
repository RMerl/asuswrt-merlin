/* BeginSourceFile tls.c

  This file creates and deletes threads. It uses thread local storage
  variables too. */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define N_THREADS 3

/* Uncomment to turn on debugging output */
/*#define START_DEBUG*/

/* Thread-local storage.  */
__thread int a_thread_local;
__thread int another_thread_local;

/* Global variable just for info addr in gdb.  */
int a_global;

/* Print the results of thread-local storage.  */
int thread_local_val[ N_THREADS ];
int another_thread_local_val[ N_THREADS ];

/* Semaphores to make sure the threads are alive when we print the TLS
   variables from gdb.  */
sem_t tell_main, tell_thread;


void print_error ()
{
  switch (errno)
  {
    case EAGAIN:
      fprintf (stderr, "EAGAIN\n");
      break;
    case EINTR:
      fprintf (stderr, "EINTR\n");
      break;
    case EINVAL:
      fprintf (stderr, "EINVAL\n");
      break;
    case ENOSYS:
      fprintf (stderr, "ENOSYS\n");
      break;
    case ENOENT:
      fprintf (stderr, "ENOENT\n");
      break;
    case EDEADLK:
      fprintf (stderr, "EDEADLK\n");
      break;
    default:
      fprintf (stderr, "Unknown error\n");
      break;
   } 
}

/* Routine for each thread to run, does nothing.  */
void *spin( vp )
    void * vp;
{
    int me = (long) vp;
    int i;
    
    /* Use a_global. */
    a_global++;

    a_thread_local = 0;
    another_thread_local = me;
    for( i = 0; i <= me; i++ ) {
        a_thread_local += i;
    }

    another_thread_local_val[me] = another_thread_local;
    thread_local_val[ me ] = a_thread_local; /* here we know tls value */

    if (sem_post (&tell_main) == -1)
     {
        fprintf (stderr, "th %d post on sem tell_main failed\n", me);
        print_error ();
        return;
     }
#ifdef START_DEBUG
    fprintf (stderr, "th %d post on tell main\n", me);
#endif

    while (1)
      {
#ifdef START_DEBUG
        fprintf (stderr, "th %d start wait on tell_thread\n", me);
#endif
        if (sem_wait (&tell_thread) == 0)
          break;

        if (errno == EINTR)
          {
#ifdef START_DEBUG
            fprintf (stderr, "th %d wait tell_thread got EINTR, rewaiting\n", me);
#endif
            continue;
          }
        else
          {  
            fprintf (stderr, "th %d wait on sem tell_thread failed\n", me);
            print_error ();
            return;
         }
      }

#ifdef START_DEBUG
      fprintf (stderr, "th %d Wait on tell_thread\n", me);
#endif

}

void
do_pass()
{
    int i;
    pthread_t t[ N_THREADS ];
    int err;

    for( i = 0; i < N_THREADS; i++)
    {
        thread_local_val[i] = 0;
        another_thread_local_val[i] = 0;
    }
   
    if (sem_init (&tell_main, 0, 0) == -1)
    {
      fprintf (stderr, "tell_main semaphore init failed\n");
      return;
    }

    if (sem_init (&tell_thread, 0, 0) == -1)
    {
      fprintf (stderr, "tell_thread semaphore init failed\n");
      return;
    }

    /* Start N_THREADS threads, then join them so that they are terminated.  */
    for( i = 0; i < N_THREADS; i++ )
     {
        err = pthread_create( &t[i], NULL, spin, (void *) (long) i );
        if( err != 0 ) {
            fprintf(stderr, "Error in thread %d create\n", i );
        }
     }

    for( i = 0; i < N_THREADS; i++ )
      {
        while (1)
          {
#ifdef START_DEBUG
            fprintf (stderr, "main %d start wait on tell_main\n", i);
#endif
            if (sem_wait (&tell_main) == 0)
              break;

            if (errno == EINTR)
              {
#ifdef START_DEBUG
                fprintf (stderr, "main %d wait tell_main got EINTR, rewaiting\n", i);
#endif
                continue;
              }
            else
              {
                fprintf (stderr, "main %d wait on sem tell_main failed\n", i);
                print_error ();
                return;
              }
            }
       }

#ifdef START_DEBUG
    fprintf (stderr, "main done waiting on tell_main\n");
#endif

    i = 10;  /* Here all threads should be still alive. */

    for( i = 0; i < N_THREADS; i++ )
     {
       if (sem_post (&tell_thread) == -1)
        {
           fprintf (stderr, "main %d post on sem tell_thread failed\n", i);
           print_error ();
           return;
        }
#ifdef START_DEBUG
      fprintf (stderr, "main %d post on tell_thread\n", i);
#endif
     }

    for( i = 0; i < N_THREADS; i++ )
     {
        err = pthread_join(t[i], NULL );
        if( err != 0 )
         { 
           fprintf (stderr, "error in thread %d join\n", i );
         }
    }

    i = 10;  /* Null line for setting bpts on. */
   
}

int
main()
{
   do_pass ();

   return 0;  /* Set breakpoint here before exit. */
}

/* EndSourceFile */
