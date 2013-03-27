#include <stdio.h>
#include <signal.h>

#ifdef __sh__
#define signal(a,b)	/* Signals not supported on this target - make them go away */
#endif


#ifdef PROTOTYPES
void
handle_USR1 (int sig)
{
}
#else
void
handle_USR1 (sig)
     int sig;
{
}
#endif

int value;

#ifdef PROTOTYPES
int
main (void)
#else
int
main ()
#endif
{
  int my_array[3] = { 1, 2, 3 };
  
  value = 7;
  
#ifdef SIGUSR1
  signal (SIGUSR1, handle_USR1);
#endif

  printf ("value is %d\n", value);
  printf ("my_array[2] is %d\n", my_array[2]);
  
  {
    int i;
    for (i = 0; i < 5; i++)
      value++;
  }

  return 0;
}

